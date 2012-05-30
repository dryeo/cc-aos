/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var useCustomPrefs;
var requestAlways;
var gIdentity;

function onInit() 
{
  useCustomPrefs = document.getElementById("identity.dsn_use_custom_prefs");
  requestAlways = document.getElementById("identity.dsn_always_request_on");

  EnableDisableCustomSettings();

  return true;
}

function onSave()
{
}

function EnableDisableCustomSettings() {
  if (useCustomPrefs && (useCustomPrefs.getAttribute("value") == "false"))
    requestAlways.setAttribute("disabled", "true");
  else
    requestAlways.removeAttribute("disabled");

  return true;
}

function onPreInit(account, accountValues)
{
  gIdentity = account.defaultIdentity;
}
