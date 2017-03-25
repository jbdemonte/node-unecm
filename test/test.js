const fs = require('fs');
const path = require('path');
const unzip = require('unzip');
const md5File = require('md5-file');
var unecm = require('../index');

const ZIP_FILE = path.join(__dirname, 'asset', 'test.iso.ecm.zip');

const ECM_FILE = path.join(__dirname, 'asset', 'test.iso.ecm');
const ECM_MD5 = '8f7130412b58500d04f9b238ad2ba866';
const ECM_SIZE = 8811648;

const ISO_FILE = path.join(__dirname, 'asset', 'test.iso');
const ISO_MD5 = '35788e782841fe2afd3d89d2bcd03438';
const ISO_SIZE = 10020864;

const RENAMED_FILE = path.join(__dirname, 'asset', 'renamed.iso');

function clean() {
  [ECM_FILE, ISO_FILE, RENAMED_FILE].forEach(function (file) {
    try {
      fs.unlinkSync(file);
    } catch (err) {}
  });
}

describe('Normal use', function () {

  beforeAll(function () {
    clean();
    return new Promise(function (resolve, reject) {
      var stream = fs
        .createReadStream(ZIP_FILE)
        .pipe(unzip.Extract({ path: path.join(__dirname, 'asset') }));

      stream.on('error', reject);

      stream.on('close', function () {
        md5File(ECM_FILE, function (err, hash) {
          if (err) {
            reject(err);
          } else if (hash !== ECM_MD5) {
            reject('MD5 mismatch');
          } else {
            resolve();
          }
        });
      });
    });
  });

  afterAll(clean);

  test('unecm test.iso.ecm', function (done) {
    var emitter = unecm(ECM_FILE);
    var progressions = [];

    emitter.on('error', function (data) {
      done(data.error || 'unknown error');
    });

    emitter.on('progress', function (data) {
      progressions.push(data.progression);
    });

    emitter.on('complete', function (data) {
      expect(data).toEqual({inLength: ECM_SIZE, outLength: ISO_SIZE});
      expect(progressions.length).toBeGreaterThan(0);
      expect(progressions.length).toBeLessThanOrEqual(100);
      progressions.forEach(function (progress) {
        expect(progress).toBeGreaterThan(0);
        expect(progress).toBeLessThanOrEqual(100);
      });

      md5File(ISO_FILE, function (err, hash) {
        expect(hash).toBe(ISO_MD5);
        done();
      });
    });

  });

  test('unecm test.iso.ecm renamed.iso', function (done) {
    var emitter = unecm(ECM_FILE, RENAMED_FILE);
    var progressions = [];

    emitter.on('error', function (data) {
      done(data.error || 'unknown error');
    });

    emitter.on('progress', function (data) {
      progressions.push(data.progression);
    });

    emitter.on('complete', function (data) {
      expect(data).toEqual({inLength: ECM_SIZE, outLength: ISO_SIZE});
      expect(progressions.length).toBeGreaterThan(0);
      expect(progressions.length).toBeLessThanOrEqual(100);
      progressions.forEach(function (progress) {
        expect(progress).toBeGreaterThan(0);
        expect(progress).toBeLessThanOrEqual(100);
      });

      md5File(RENAMED_FILE, function (err, hash) {
        expect(hash).toBe(ISO_MD5);
        done();
      });
    });

  });

  test('unecm test.iso', function (done) {
  var emitter = unecm(ISO_FILE);
  var progressions = [];

  emitter.on('error', function (data) {
    done(data.error || 'unknown error');
  });

  emitter.on('progress', function (data) {
    progressions.push(data.progression);
  });

  emitter.on('complete', function (data) {
    expect(data).toEqual({inLength: ECM_SIZE, outLength: ISO_SIZE});
    expect(progressions.length).toBeGreaterThan(0);
    expect(progressions.length).toBeLessThanOrEqual(100);
    progressions.forEach(function (progress) {
      expect(progress).toBeGreaterThan(0);
      expect(progress).toBeLessThanOrEqual(100);
    });

    md5File(ISO_FILE, function (err, hash) {
      expect(hash).toBe(ISO_MD5);
      done();
    });
  });

});

});

describe('Bad use', function () {

  test('unecm without parameters', function () {
    expect(unecm).toThrow(new Error('inFilename required as a string'));
  });

  test('unecm with bad input in inFilename', function () {
    expect(function () {
      unecm({});
    }).toThrow(new Error('inFilename required as a string'));
  });

  test('unecm with bad input in outFilename', function () {
    expect(function () {
      unecm('test.ecm', {});
    }).toThrow(new Error('outFilename has to be a string'));
  });

  test('unecm bad in filename', function () {
    expect(function () {
      unecm('test.zip', 'test.iso');
    }).toThrow(new Error('inFilename must end in .ecm'));
  });

  test('unecm unknown file', function (done) {
    var emitter = unecm(ECM_FILE);

    emitter.on('error', function (data) {
      expect(data).toEqual({error: "Error opening source file in read (rb) mode"});
      done();
    });

    emitter.on('progress', function () {
      done('Should have failed');
    });

    emitter.on('complete', function () {
      done('Should have failed');
    });

  });

});