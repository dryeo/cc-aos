/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsMsgAppCore_h
#define __nsMsgAppCore_h

#include "nscore.h"
#include "nsIMessenger.h"
#include "nsCOMPtr.h"
#include "nsITransactionManager.h"
#include "nsILocalFile.h"
#include "nsIDocShell.h"
#include "nsIStringBundle.h"
#include "nsILocalFile.h"
#include "nsWeakReference.h"
#include "nsIDOMWindow.h"

class nsMessenger : public nsIMessenger, public nsSupportsWeakReference, public nsIFolderListener
{

public:
  nsMessenger();
  virtual ~nsMessenger();

  NS_DECL_ISUPPORTS  
  NS_DECL_NSIMESSENGER
  NS_DECL_NSIFOLDERLISTENER

  nsresult Alert(const char * stringName);

  nsresult SaveAttachment(nsIFile *file, const nsACString& unescapedUrl,
                          const nsACString& messageUri, const nsACString& contentType, 
                          void *closure, nsIUrlListener *aListener);
  nsresult PromptIfFileExists(nsILocalFile *file);
  nsresult DetachAttachments(PRUint32 aCount,
                             const char ** aContentTypeArray,
                             const char ** aUrlArray,
                             const char ** aDisplayNameArray,
                             const char ** aMessageUriArray,
                             nsTArray<nsCString> *saveFileUris,
                             bool withoutWarning = false);
  nsresult SaveAllAttachments(PRUint32 count,
                              const char **contentTypeArray,
                              const char **urlArray,
                              const char **displayNameArray,
                              const char **messageUriArray,
                              bool detaching);
  nsresult SaveOneAttachment(const char* aContentType,
                             const char* aURL,
                             const char* aDisplayName,
                             const char* aMessageUri,
                             bool detaching);

protected:
  void GetString(const nsString& aStringName, nsString& stringValue);
  nsresult InitStringBundle();
  nsresult PromptIfDeleteAttachments(bool saveFirst, PRUint32 count, const char **displayNameArray);

private:
  nsresult GetLastSaveDirectory(nsILocalFile **aLastSaveAsDir);
  // if aLocalFile is a dir, we use it.  otherwise, we use the parent of aLocalFile.
  nsresult SetLastSaveDirectory(nsILocalFile *aLocalFile);

  nsresult GetSaveAsFile(const nsAString& aMsgFilename, PRInt32 *aSaveAsFileType,
                         nsILocalFile **aSaveAsFile);

  nsresult GetSaveToDir(nsILocalFile **aSaveToDir);

  nsString mId;
  nsCOMPtr<nsITransactionManager> mTxnMgr;

  /* rhp - need this to drive message display */
  nsCOMPtr<nsIDOMWindow>    mWindow;
  nsCOMPtr<nsIMsgWindow>    mMsgWindow;
  nsCOMPtr<nsIDocShell>     mDocShell;

  // String bundles...
  nsCOMPtr<nsIStringBundle>   mStringBundle;

  nsCString mCurrentDisplayCharset;

  nsCOMPtr<nsISupports>  mSearchContext;
  nsCString   mLastDisplayURI; // this used when the user attempts to force a charset reload of a message...we need to get the last displayed
                               // uri so we can re-display it..
  nsCString mNavigatingToUri;
  nsTArray<nsCString> mLoadedMsgHistory;
  PRInt32 mCurHistoryPos;
};

#define NS_MESSENGER_CID \
{ /* f436a174-e2c0-4955-9afe-e3feb68aee56 */      \
  0xf436a174, 0xe2c0, 0x4955,                     \
    {0x9a, 0xfe, 0xe3, 0xfe, 0xb6, 0x8a, 0xee, 0x56}}

#endif
