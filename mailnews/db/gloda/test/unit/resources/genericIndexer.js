/**
 * Generic indexing mechanism; does nothing special, just uses
 *  Gloda.grokNounItem.  Call GenericIndexer.indexNewObjects() to queue
 *  queue your objects for initial indexing.
 */
var GenericIndexer = {
  _log: Log4Moz.repository.getLogger("gloda.indexer.generic"),
  /* public interface */
  name: "generic_indexer",
  enable: function() {
    this.enabled = true;
  },
  disable: function() {
    this.enabled = false;
  },
  get workers() {
    return [["generic-new", this._worker_index_generic_new]];
  },
  initialSweep: function() {
  },
  /* mock interface */
  enabled: false,
  initialSweepCalled: false,
  indexNewObjects: function(aObjects) {
    indexingInProgress = true;
    this._log.debug("enqueuing " + aObjects.length +
      " new generic objects with id: " + aObjects[0].NOUN_ID);
    GlodaIndexer.indexJob(new IndexingJob("generic-new", 0, null, aObjects.concat()));
  },
  /* implementation */
  _worker_index_generic_new: function(aJob, aCallbackHandle) {
    this._log.debug("Beginning indexing " + aJob.items.length + " generic new items");
    for (let [, item] in Iterator(aJob.items)) {
      this._log.debug("Indexing: " + item);
      yield aCallbackHandle.pushAndGo(
        Gloda.grokNounItem(item, {}, true, true, aCallbackHandle,
                           item.NOUN_DEF.cache));
    }

    yield GlodaIndexer.kWorkDone;
    this._log.debug("Done indexing");
  }
};
GlodaIndexer.registerIndexer(GenericIndexer);

let indexingInProgress = false;
function genericIndexerCallback(aStatus) {
  // If indexingInProgress is false, we've received the synthetic
  // notification, so ignore it
  if (indexingInProgress && aStatus == Gloda.kIndexerIdle) {
    indexingInProgress = false;
    next_test();
  }
}
Gloda.addIndexerListener(genericIndexerCallback);
