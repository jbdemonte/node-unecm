#include <nan.h>
using namespace Nan;
using namespace v8;

Local<Function> onProgress;
Local<Function> onError;
Local<Function> onComplete;

void emitProgress(int progression) {
  v8::Local<v8::Value> argv[1] = {
    Nan::New(progression)
  };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), onProgress, 1, argv);
}

void emitComplete(int32_t inLength, int32_t outLength) {
  v8::Local<v8::Value> argv[2] = {
    Nan::New(inLength),
    Nan::New(outLength)
  };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), onComplete, 2, argv);
}

void emitError(std::string const& message) {
  v8::Local<v8::Value> argv[1] = {
    Nan::New(message).ToLocalChecked()
  };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), onError, 1, argv);
}

/***************************************************************************/

/* Data types */
typedef unsigned char ecc_uint8;
typedef unsigned short ecc_uint16;
typedef unsigned int ecc_uint32;

/* LUTs used for computing ECC/EDC */
static ecc_uint8 ecc_f_lut[256];
static ecc_uint8 ecc_b_lut[256];
static ecc_uint32 edc_lut[256];

/* Init routine */
static void eccedc_init(void) {
  ecc_uint32 i, j, edc;
  for(i = 0; i < 256; i++) {
    j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
    ecc_f_lut[i] = j;
    ecc_b_lut[i ^ j] = i;
    edc = i;
    for(j = 0; j < 8; j++) edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
    edc_lut[i] = edc;
  }
}

/***************************************************************************/
/*
** Compute EDC for a block
*/
ecc_uint32 edc_partial_computeblock(
        ecc_uint32  edc,
  const ecc_uint8  *src,
        ecc_uint16  size
) {
  while(size--) edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
  return edc;
}

void edc_computeblock(
  const ecc_uint8  *src,
        ecc_uint16  size,
        ecc_uint8  *dest
) {
  ecc_uint32 edc = edc_partial_computeblock(0, src, size);
  dest[0] = (edc >>  0) & 0xFF;
  dest[1] = (edc >>  8) & 0xFF;
  dest[2] = (edc >> 16) & 0xFF;
  dest[3] = (edc >> 24) & 0xFF;
}

/***************************************************************************/
/*
** Compute ECC for a block (can do either P or Q)
*/
static void ecc_computeblock(
  ecc_uint8 *src,
  ecc_uint32 major_count,
  ecc_uint32 minor_count,
  ecc_uint32 major_mult,
  ecc_uint32 minor_inc,
  ecc_uint8 *dest
) {
  ecc_uint32 size = major_count * minor_count;
  ecc_uint32 major, minor;
  for(major = 0; major < major_count; major++) {
    ecc_uint32 index = (major >> 1) * major_mult + (major & 1);
    ecc_uint8 ecc_a = 0;
    ecc_uint8 ecc_b = 0;
    for(minor = 0; minor < minor_count; minor++) {
      ecc_uint8 temp = src[index];
      index += minor_inc;
      if(index >= size) index -= size;
      ecc_a ^= temp;
      ecc_b ^= temp;
      ecc_a = ecc_f_lut[ecc_a];
    }
    ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
    dest[major              ] = ecc_a;
    dest[major + major_count] = ecc_a ^ ecc_b;
  }
}

/*
** Generate ECC P and Q codes for a block
*/
static void ecc_generate(
  ecc_uint8 *sector,
  int        zeroaddress
) {
  ecc_uint8 address[4], i;
  /* Save the address and zero it out */
  if(zeroaddress) for(i = 0; i < 4; i++) {
    address[i] = sector[12 + i];
    sector[12 + i] = 0;
  }
  /* Compute ECC P code */
  ecc_computeblock(sector + 0xC, 86, 24,  2, 86, sector + 0x81C);
  /* Compute ECC Q code */
  ecc_computeblock(sector + 0xC, 52, 43, 86, 88, sector + 0x8C8);
  /* Restore the address */
  if(zeroaddress) for(i = 0; i < 4; i++) sector[12 + i] = address[i];
}

/***************************************************************************/
/*
** Generate ECC/EDC information for a sector (must be 2352 = 0x930 bytes)
** Returns 0 on success
*/
void eccedc_generate(ecc_uint8 *sector, int type) {
  ecc_uint32 i;
  switch(type) {
  case 1: /* Mode 1 */
    /* Compute EDC */
    edc_computeblock(sector + 0x00, 0x810, sector + 0x810);
    /* Write out zero bytes */
    for(i = 0; i < 8; i++) sector[0x814 + i] = 0;
    /* Generate ECC P/Q codes */
    ecc_generate(sector, 0);
    break;
  case 2: /* Mode 2 form 1 */
    /* Compute EDC */
    edc_computeblock(sector + 0x10, 0x808, sector + 0x818);
    /* Generate ECC P/Q codes */
    ecc_generate(sector, 1);
    break;
  case 3: /* Mode 2 form 2 */
    /* Compute EDC */
    edc_computeblock(sector + 0x10, 0x91C, sector + 0x92C);
    break;
  }
}

/***************************************************************************/

unsigned long mycounter;
unsigned long mycounter_total;

void resetcounter(unsigned long total) {
  mycounter = 0;
  mycounter_total = total;
}

void setcounter(unsigned long n) {
  if((n >> 20) != (mycounter >> 20)) {
    unsigned long a = (n+64)/128;
    unsigned long d = (mycounter_total+64)/128;
    if(!d) d = 1;
    emitProgress((100*a) / d);
  }
  mycounter = n;
}

int unecmify(
  FILE *in,
  FILE *out
) {
  unsigned checkedc = 0;
  unsigned char sector[2352];
  unsigned type;
  unsigned num;
  fseek(in, 0, SEEK_END);
  resetcounter(ftell(in));
  fseek(in, 0, SEEK_SET);
  if(
    (fgetc(in) != 'E') ||
    (fgetc(in) != 'C') ||
    (fgetc(in) != 'M') ||
    (fgetc(in) != 0x00)
  ) {
    goto corrupt;
  }
  for(;;) {
    int c = fgetc(in);
    int bits = 5;
    if(c == EOF) goto uneof;
    type = c & 3;
    num = (c >> 2) & 0x1F;
    while(c & 0x80) {
      c = fgetc(in);
      if(c == EOF) goto uneof;
      num |= ((unsigned)(c & 0x7F)) << bits;
      bits += 7;
    }
    if(num == 0xFFFFFFFF) break;
    num++;
    if(num >= 0x80000000) goto corrupt;
    if(!type) {
      while(num) {
        int b = num;
        if(b > 2352) b = 2352;
        if(fread(sector, 1, b, in) != b) goto uneof;
        checkedc = edc_partial_computeblock(checkedc, sector, b);
        fwrite(sector, 1, b, out);
        num -= b;
        setcounter(ftell(in));
      }
    } else {
      while(num--) {
        memset(sector, 0, sizeof(sector));
        memset(sector + 1, 0xFF, 10);
        switch(type) {
        case 1:
          sector[0x0F] = 0x01;
          if(fread(sector + 0x00C, 1, 0x003, in) != 0x003) goto uneof;
          if(fread(sector + 0x010, 1, 0x800, in) != 0x800) goto uneof;
          eccedc_generate(sector, 1);
          checkedc = edc_partial_computeblock(checkedc, sector, 2352);
          fwrite(sector, 2352, 1, out);
          setcounter(ftell(in));
          break;
        case 2:
          sector[0x0F] = 0x02;
          if(fread(sector + 0x014, 1, 0x804, in) != 0x804) goto uneof;
          sector[0x10] = sector[0x14];
          sector[0x11] = sector[0x15];
          sector[0x12] = sector[0x16];
          sector[0x13] = sector[0x17];
          eccedc_generate(sector, 2);
          checkedc = edc_partial_computeblock(checkedc, sector + 0x10, 2336);
          fwrite(sector + 0x10, 2336, 1, out);
          setcounter(ftell(in));
          break;
        case 3:
          sector[0x0F] = 0x02;
          if(fread(sector + 0x014, 1, 0x918, in) != 0x918) goto uneof;
          sector[0x10] = sector[0x14];
          sector[0x11] = sector[0x15];
          sector[0x12] = sector[0x16];
          sector[0x13] = sector[0x17];
          eccedc_generate(sector, 3);
          checkedc = edc_partial_computeblock(checkedc, sector + 0x10, 2336);
          fwrite(sector + 0x10, 2336, 1, out);
          setcounter(ftell(in));
          break;
        }
      }
    }
  }
  if(fread(sector, 1, 4, in) != 4) goto uneof;
  if(
    (sector[0] != ((checkedc >>  0) & 0xFF)) ||
    (sector[1] != ((checkedc >>  8) & 0xFF)) ||
    (sector[2] != ((checkedc >> 16) & 0xFF)) ||
    (sector[3] != ((checkedc >> 24) & 0xFF))
  ) {
    goto corrupt;
  }
  emitProgress(100);
  emitComplete(ftell(in), ftell(out));
  return 0;
uneof:
  emitError("Unexpected EOF!");
corrupt:
  emitError("Corrupt ECM file!");
  return 1;
}

/***************************************************************************/

void unecm(const Nan::FunctionCallbackInfo<v8::Value>& info) {

  String::Utf8Value fileIn(info[0]->ToString());
  String::Utf8Value fileOut(info[1]->ToString());
  onProgress = info[2].As<Function>();
  onComplete = info[3].As<Function>();
  onError = info[4].As<Function>();

  FILE *fin;
  FILE *fout;

  fin = fopen((const char*)(*fileIn), "rb");
  if (!fin) {
    emitError("Error opening source file in read (rb) mode");
    info.GetReturnValue().Set(-1);
    return ;
  }

  fout = fopen((const char*)(*fileOut), "wb");
  if (!fout) {
    emitError("Error opening destination file in write (wb) mode");
    info.GetReturnValue().Set(-1);
    return ;
  }

  eccedc_init();

  unecmify(fin, fout);

  fclose(fin);
  fclose(fout);

  info.GetReturnValue().Set(0);
}


void Init(v8::Local<v8::Object> exports) {
  exports->Set(Nan::New("unecm").ToLocalChecked(),
               Nan::New<v8::FunctionTemplate>(unecm)->GetFunction());
}

NODE_MODULE(addon, Init)