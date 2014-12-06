const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cm = Components.manager;

function checkRegistrar()
{
  let localeService = Cc["@mozilla.org/intl/nslocaleservice;1"]
                        .getService(Ci.nsILocaleService);
  let systemLocale = localeService.getSystemLocale();
  do_check_eq("@mozilla.org/messenger/collation-registrar;1" in Cc,
              systemLocale.getCategory("NSILOCALE_COLLATE").startsWith("ja"));
}

function run_test()
{
  checkRegistrar();
}
