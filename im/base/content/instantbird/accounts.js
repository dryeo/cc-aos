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
 * The Original Code is the Instantbird messenging client, released
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// This is the list of notifications that the account manager window observes
const events = [
  "purple-quit",
  "account-added",
  "account-updated",
  "account-removed",
  "account-connected",
  "account-connecting",
  "account-disconnected",
  "account-disconnecting",
  "account-connect-progress",
  "account-connect-error",
  "autologin-processed"
];

var gAccountManager = {
  load: function am_load() {
    this.accountList = document.getElementById("accountlist");
    for (let acc in this.getAccounts()) {
      var elt = document.createElement("richlistitem");
      this.accountList.appendChild(elt);
      elt.build(acc);
    }
    addObservers(this, events);
    if (!this.accountList.getRowCount())
      // This is horrible, but it works. Otherwise (at least on mac)
      // the wizard is not centered relatively to the account manager
      setTimeout(function() { gAccountManager.new(); }, 0);
    else
      this.accountList.selectedIndex = 0;

    this.setAutoLoginNotification();

    window.addEventListener("unload", this.unload, false);
  },
  unload: function am_unload() {
    removeObservers(gAccountManager, events);
  },
  observe: function am_observe(aObject, aTopic, aData) {
    if (aTopic == "purple-quit") {
      // libpurple is being uninitialized. We don't need the account
      // manager window anymore, close it.
      this.close();
      return;
    }
    else if (aTopic == "autologin-processed") {
      var notification = document.getElementById("accountsNotificationBox")
                                 .getNotificationWithValue("autoLoginStatus");
      if (notification)
        notification.close();
      return;
    }

    if (!(aObject instanceof Ci.purpleIAccount))
      throw "Bad notification.";

    if (aTopic == "account-added") {
      var elt = document.createElement("richlistitem");
      this.accountList.appendChild(elt);
      elt.build(aObject);
      if (this.accountList.getRowCount() == 1)
        this.accountList.selectedIndex = 0;
    }
    else if (aTopic == "account-removed") {
      var elt = document.getElementById(aObject.id);
      if (!elt.selected) {
        this.accountList.removeChild(elt);
        return;
      }
      // The currently selected element is removed,
      // ensure another element gets selected (if the list is not empty)
      var selectedIndex = this.accountList.selectedIndex;
      this.accountList.removeChild(elt);
      var count = this.accountList.getRowCount();
      if (!count)
        return;
      if (selectedIndex == count)
        --selectedIndex;
      this.accountList.selectedIndex = selectedIndex;
    }
    else if (aTopic == "account-updated") {
      var elt = document.getElementById(aObject.id);
      elt.build(aObject);
      return;
    }

    const stateEvents = {
      "account-connected": "connected",
      "account-connecting": "connecting",
      "account-disconnected": "disconnected",
      "account-disconnecting": "disconnecting"
    };
    var elt = document.getElementById(aObject.id);
    if (aTopic in stateEvents) {
      if (!elt) {
        /* The listitem associated with this account could not be
        found. This happens when the account is being deleted. The
        account-removed signal is fired before account-disconnecting
        and account-disconnected. Maybe we should add a readonly
        boolean attribute |deleting| to purpleIAccount? */
        return;
      }

      /* handle protocol icon animation while connecting */
      var icon = document.getAnonymousElementByAttribute(elt, "anonid", "prplicon");
      if (aTopic == "account-connecting") {
        icon.animate();
        elt.removeAttribute("error");
        aObject.connectionStateMsg = "";
        elt.updateConnectionState(false);
      }
      else
        icon.stop();

      elt.setAttribute("state", stateEvents[aTopic]);
    }
    else if (aTopic == "account-connect-progress") {
      elt.updateConnectionState(false);
    }
    else if (aTopic == "account-connect-error") {
      elt.updateConnectionState(true);
    }
  },
  connect: function am_connect() {
    this.accountList.selectedItem.connect();
  },
  disconnect: function am_disconnect() {
    this.accountList.selectedItem.disconnect();
  },
  delete: function am_delete() {
    this.accountList.selectedItem.delete();
  },
  new: function am_new() {
    this.openDialog("chrome://instantbird/content/accountWizard.xul");
  },
  edit: function am_edit() {
    this.openDialog("chrome://instantbird/content/account.xul",
                    this.accountList.selectedItem.account);
  },
  autologin: function am_autologin() {
    var elt = this.accountList.selectedItem;
    elt.autoLogin = !elt.autoLogin;
  },
  close: function am_close() {
    // If a modal dialog is opened, we can't close this window now
    if (this.modalDialog)
      setTimeout(function() { window.close();}, 0);
    else
      window.close();
  },

  selectAccount: function am_selectAccount(aAccountId) {
    this.accountList.selectedItem = document.getElementById(aAccountId);
    this.accountList.ensureSelectedElementIsVisible();
  },

  getAccounts: function am_getAccounts() {
    var pcs = Components.classes["@instantbird.org/purple/core;1"]
                        .getService(Ci.purpleICoreService);
    return getIter(pcs.getAccounts, Ci.purpleIAccount);
  },

  openDialog: function am_openDialog(aUrl, aArgs) {
    this.modalDialog = true;
    window.openDialog(aUrl, "",
                      "chrome,modal,titlebar,centerscreen",
                      aArgs);
    this.modalDialog = false;
  },
  setAutoLoginNotification: function am_setAutoLoginNotification() {
    var pcs = Components.classes["@instantbird.org/purple/core;1"]
                        .getService(Ci.purpleICoreService);
    var autoLoginStatus = pcs.autoLoginStatus;

    if (autoLoginStatus == pcs.AUTOLOGIN_ENABLED)
      return;

    var bundle = document.getElementById("accountsBundle");
    var box = document.getElementById("accountsNotificationBox");
    var priority = box.PRIORITY_INFO_HIGH;
    var label;

    switch (autoLoginStatus) {
      case pcs.AUTOLOGIN_USER_DISABLED:
        label = bundle.getString("accountsManager.notification.userDisabled.label");
        break;

      case pcs.AUTOLOGIN_SAFE_MODE:
        label = bundle.getString("accountsManager.notification.safeMode.label");
        break;

      case pcs.AUTOLOGIN_START_OFFLINE:
        label = bundle.getString("accountsManager.notification.startOffline.label");
        break;

      case pcs.AUTOLOGIN_CRASH:
        label = bundle.getString("accountsManager.notification.crash.label");
        priority = box.PRIORITY_WARNING_MEDIUM;
        break;

      default:
        label = bundle.getString("accountsManager.notification.other.label");
    }

    var connectNowButton = {
      accessKey: bundle.getString("accountsManager.notification.button.accessKey"),
      callback: this.processAutoLogin,
      label: bundle.getString("accountsManager.notification.button.label")
    }

    box.appendNotification(label, "autologinStatus", null, priority, [connectNowButton]);
  },
  processAutoLogin: function am_processAutoLogin() {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService2);
    if (ioService.offline) {
      ioService.manageOfflineStatus = false;
      ioService.offline = false;
    }

    Components.classes["@instantbird.org/purple/core;1"]
              .getService(Ci.purpleICoreService)
              .processAutoLogin();
  }
};
