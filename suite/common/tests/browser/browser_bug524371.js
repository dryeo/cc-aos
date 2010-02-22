/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Daniel Witte <dwitte@mozilla.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test() {
  // test that cookies are stored and restored correctly by sessionstore,
  // bug 524371.

  // test setup
  waitForExplicitFinish();

  let cs = Components.classes["@mozilla.org/cookiemanager;1"].getService(Components.interfaces.nsICookieManager2);
  cs.removeAll();

  let ss = Components.classes["@mozilla.org/suite/sessionstore;1"].getService(Components.interfaces.nsISessionStore);

  var gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefBranch);

  // make sure that sessionstore.js can be forced to be created by setting
  // the interval pref to 0
  gPrefService.setIntPref("browser.sessionstore.interval", 0);

  const testURL = "http://localhost:8888/browser/" +
    "suite/common/tests/browser/browser_bug524371_sample.html";

  // open a new window
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no", "about:blank");

  // make sure sessionstore saves the cookie data, then close the window
  newWin.addEventListener("load", function (aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    newWin.getBrowser().selectedBrowser.loadURI(testURL, null, null);

    newWin.getBrowser().addEventListener("pageshow", function (aEvent) {
      newWin.getBrowser().removeEventListener("pageshow", arguments.callee, true);

      // get the sessionstore state for the window
      let state = ss.getWindowState(newWin);

      // verify our cookie got set during pageload
      let e = cs.enumerator;
      let cookie;
      let i = 0;
      while (e.hasMoreElements()) {
        cookie = e.getNext().QueryInterface(Components.interfaces.nsICookie);
       i++;
      }
      is(i, 1, "expected one cookie");

      // remove the cookie
      cs.removeAll();

      // restore the window state
      ss.setWindowState(newWin, state, true);

      // at this point, the cookie should be restored...
      e = cs.enumerator;
      let cookie2;
      while (e.hasMoreElements()) {
        cookie2 = e.getNext().QueryInterface(Components.interfaces.nsICookie);
        if (cookie.name == cookie2.name)
          break;
      }
      is(cookie.name, cookie2.name, "cookie name successfully restored");
      is(cookie.value, cookie2.value, "cookie value successfully restored");
      is(cookie.path, cookie2.path, "cookie path successfully restored");

      // clean up
      gPrefService.clearUserPref("browser.sessionstore.interval");
      cs.removeAll();
      newWin.close();
      finish();
    }, true);
  }, false);
}
