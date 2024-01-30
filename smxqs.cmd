/*****************************************************************************/
/* smxqs.cmd - creates .xqs files for Seamonkey                              */
/* placed in the Public Domain 2010-08-07 by the author Richard L Walsh      */
/*****************************************************************************/

/* change these 3 lines to match your setup */

srcRoot = 'g:\work\cc45-git\obj-sm'
srcExe = 'suite\app\seamonkey.exe'
/* srcExe1 = 'mozilla\ipc\app\plugin-container.exe' */
/* srcExe2 = 'mozilla\toolkit\mozapps\update\updater\updater.exe' */
dst = 'g:\work\cc45-git\obj-sm\dist\bin'

/*****************************************************************************/

Main:
  call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
  call SysLoadFuncs

  oldDir = directory()
  newDir = directory(dst)
  noSym.0 = 0

  /* process all the subdirectories except for "dist" */
  call SysFileTree srcRoot || '\*', 'dirs', 'DO'
  do ctr = 1 to dirs.0
    parse upper var dirs.ctr curDir
    if right(curDir,5) = '\DIST' then
      iterate
    call ProcessDir(dirs.ctr)
  end

  /* create an .xqs file for the executable */
  call ProcessFile(srcRoot || '\' || srcExe)
/*  call ProcessFile(srcRoot || '\' || srcExe1)
  call ProcessFile(srcRoot || '\' || srcExe2) */

  /* list failures at the end so people can see them */
  if noSym.0 > 0 then do
    say
    say '** Error: an .xqs file could not be created for these files:'
    do ctr = 1 to noSym.0
      say noSym.ctr
    end
  end

  oldDir = directory(oldDir)
  exit

/*****************************************************************************/

ProcessDir: procedure expose dst noSym.

  parse arg curDir

  /* get a list of all the dlls in 'curDir' and its subdirectories */
  call SysFileTree curDir || '\*.dll', 'files', 'FSO'
  if files.0 = 0 then
    return

  do ctr = 1 to files.0
    call ProcessFile(files.ctr)
  end

  return

/*****************************************************************************/

ProcessFile: procedure expose dst noSym.

  parse arg srcFile

  srcMap = overlay('map', srcFile, length(srcFile) - 2)

  /* confirm that a .map file exists for this binary */
  if stream(srcMap,'c','query exists') = '' then do
    ctr = noSym.0 + 1
    noSym.ctr = ' Mapfile not found for' srcFile
    noSym.0 = ctr
    return
  end

  dstFile = dst || '\' || filespec('name', srcFile)
  dstSym = overlay('xqs', dstFile, length(dstFile) - 2)

  say ' Creating' filespec('name', dstSym)

  /* use mapxqs to generate a demangled XQS symbol file in 'dst' */
  '@mapxqs -o' dstSym srcMap
  if rc <> 0 then do
    ctr = noSym.0 + 1
    noSym.ctr = ' mapxqs failed for' srcFile
    noSym.0 = ctr
    return
  end

  /* if the binary doesn't exist in 'dst', assume it's in dst/components;
     copy the .xqs file there, then delete the original */
  if stream(dstFile, 'c', 'query exists') = '' then do
    '@copy' dstSym dst || '\components\* > NUL'
    '@del' dstSym
  end

  return

/*****************************************************************************/

