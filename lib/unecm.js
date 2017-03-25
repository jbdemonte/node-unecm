const addon = require('bindings')('addon');
const events = require('events');
const path = require('path');
var pattern = /(.*)\.ecm$/i;

module.exports = function unecm(inFilename, outFilename) {

  if (!inFilename || typeof inFilename !== 'string') {
    throw new Error('inFilename required as a string');
  }

  var match = inFilename.match(pattern);

  if (!match && outFilename) {
    throw new Error('inFilename must end in .ecm');
  }

  if (outFilename && typeof outFilename !== 'string') {
    throw new Error('outFilename has to be a string');
  }

  var eventEmitter = new events.EventEmitter();

  if (!match) {
    outFilename = inFilename;
    inFilename += '.ecm';
  } else if (!outFilename) {
    outFilename = match[1];
  }

  inFilename = path.resolve(inFilename);
  outFilename = path.resolve(outFilename);

  process.nextTick(function () {
    addon.unecm(
      inFilename,
      outFilename,
      function (progression) {
        eventEmitter.emit('progress', {progression: progression});
      },
      function (inLength, outLength) {
        eventEmitter.emit('complete', {inLength: inLength, outLength: outLength});
      },
      function (error) {
        eventEmitter.emit('error', {error: error});
      }
    );
  });

  return eventEmitter;
};