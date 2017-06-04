/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Error url MUST be formatted like this:
//   about:blocked?e=error_code&u=url

// Note that this file uses document.documentURI to get
// the URL (with the format from above). This is because
// document.location.href gets the current URI off the docshell,
// which is the URL displayed in the location bar, i.e.
// the URI that the user attempted to load.

// initializing the page in this way, window.onload won't work here

initPage();

function getErrorCode()
{
  var url = document.documentURI;
  var error = url.search(/e\=/);
  var duffUrl = url.search(/\&u\=/);
  return decodeURIComponent(url.slice(error + 2, duffUrl));
}

function getURL()
{
  var url = document.documentURI;
  var match = url.match(/&u=([^&]+)&/);

  // match == null if not found; if so, return an empty string
  // instead of what would turn out to be portions of the URI
  if (!match)
    return "";

  url = decodeURIComponent(match[1]);

  // If this is a view-source page, then get then real URI of the page
  if (url.startsWith("view-source:"))
    url = url.slice(12);
  return url;
}

 /**
  * Check whether this warning page should be overridable or whether
  * the "ignore warning" button should be hidden.
  */
 function getOverride()
 {
   var url = document.documentURI;
   var match = url.match(/&o=1&/);
   return !!match;
 }

/**
 * Attempt to get the hostname via document.location.  Fail back
 * to getURL so that we always return something meaningful.
 */
function getHostString()
{
  try {
    return document.location.hostname;
  } catch (e) {
    return getURL();
  }
}

function initPage()
{
  // Handoff to the appropriate initializer, based on error code
  var error = "";
  switch (getErrorCode()) {
    case "malwareBlocked" :
      error = "malware";
      break;
    case "phishingBlocked" :
      error = "phishing";
      break;
    case "unwantedBlocked" :
      error = "unwanted";
      break;
    case "forbiddenBlocked" :
      error = "forbidden";
      break;
    default:
      return;
  }

  var el;

  if (error !== "malware") {
    el = document.getElementById("errorTitleText_malware");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorShortDescText_malware");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorLongDescText_malware");
    el.parentNode.removeChild(el);
  }

  if (error !== "phishing") {
    el = document.getElementById("errorTitleText_phishing");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorShortDescText_phishing");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorLongDescText_phishing");
    el.parentNode.removeChild(el);
  }

  if (error !== "unwanted") {
    el = document.getElementById("errorTitleText_unwanted");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorShortDescText_unwanted");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorLongDescText_unwanted");
    el.parentNode.removeChild(el);
  }

  if (error !== "forbidden") {
    el = document.getElementById("errorTitleText_forbidden");
    el.parentNode.removeChild(el);
    el = document.getElementById("errorShortDescText_forbidden");
    el.parentNode.removeChild(el);
    el = document.getElementById("whyForbiddenButton");
    el.parentNode.removeChild(el);
  } else {
    el = document.getElementById("ignoreWarningButton");
    el.parentNode.removeChild(el);
    el = document.getElementById("reportButton");
    el.parentNode.removeChild(el);

    // Remove red style: A "forbidden site" does not warrant the same level
    // of anxiety as a security concern.
    document.documentElement.className = "";
  }

  // Set sitename
  document.getElementById(error + "_sitename").textContent = getHostString();
  document.title = document.getElementById("errorTitleText_" + error)
                           .innerHTML;

  if (!getOverride()) {
    var btn = document.getElementById("ignoreWarningButton");
    if (btn) {
      btn.parentNode.removeChild(btn);
    }
  }
}

