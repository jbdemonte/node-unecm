# ECM decode utility for Node.js

[![travis build](https://img.shields.io/travis/jbdemonte/node-unecm.svg)](https://travis-ci.org/jbdemonte/node-unecm)
[![Coverage Status](https://coveralls.io/repos/github/jbdemonte/node-unecm/badge.svg?branch=master)](https://coveralls.io/github/jbdemonte/node-unecm?branch=master)
[![NPM Version](https://img.shields.io/npm/v/node-unecm.svg)](https://www.npmjs.com/package/node-unecm)

This project is a port of the ECM decode utility from [ECM](https://github.com/kidoz/ecm).

## Installation
```
npm install unecm --save
```

## Usage

```
var unecm = require('unecm')

var handler = unecm('Metal Slug X.img.ecm');

handler.on('error', function (data) {
  console.log(data);
});

handler.on('progress', function (data) {
  console.log(data);
});

handler.on('complete', function (data) {
  console.log(data);
});
```

### unecm([source], [destination])

Either the source or the destination has to be provided.
__Note: a source file must with `.ecm`, the destination can't ends with `.ecm`__

The ECM format allows you to reduce the size of a typical CD image file (BIN, CDI, NRG, CCD, or any other format that uses raw sectors; results may vary).

#### Parameters:

`source` - string - The source file. If not provided, the destination path plus the `.ecm` will be used.
`destination` - string - The destination file. If not provided, the source path minus the `.ecm` will be used.

#### Returns:

[EventEmitter](https://nodejs.org/api/events.html#events_class_eventemitter)

#### Events

##### `progress`
*Emitted during the encoding*

parameter: `data`

`data.progression` - Number - 0..100 - Progression of the decoding

##### `error`
*Emitted when an error has occurred*

parameter: `data`

`data.error` - String - Label of the error


##### `complete`
*Emitted when encoding is finished*

parameter: `data`

`data.inLength` - Number - Source length
`data.outLength` - Number - Destination length

## License
[GPL-2.0](https://opensource.org/licenses/GPL-2.0)