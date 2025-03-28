/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: scf_eda.proto */

#ifndef PROTOBUF_C_scf_5feda_2eproto__INCLUDED
#define PROTOBUF_C_scf_5feda_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1002001 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _ScfLine ScfLine;
typedef struct _ScfEpin ScfEpin;
typedef struct _ScfEconn ScfEconn;
typedef struct _ScfEline ScfEline;
typedef struct _ScfEcomponent ScfEcomponent;
typedef struct _ScfEfunction ScfEfunction;
typedef struct _ScfEboard ScfEboard;


/* --- enums --- */


/* --- messages --- */

struct  _ScfLine
{
  ProtobufCMessage base;
  int32_t x0;
  int32_t y0;
  int32_t x1;
  int32_t y1;
};
#define SCF_LINE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_line__descriptor) \
    , 0, 0, 0, 0 }


struct  _ScfEpin
{
  ProtobufCMessage base;
  uint64_t id;
  uint64_t cid;
  uint64_t lid;
  uint64_t flags;
  size_t n_tos;
  uint64_t *tos;
  uint64_t c_lid;
  double v;
  double jv;
  double a;
  double ja;
  double r;
  double jr;
  double uf;
  double uh;
  double hfe;
  double dr;
  double jdr;
  double sr;
  double jsr;
  double pr;
  double jpr;
  uint64_t path;
  int32_t x;
  int32_t y;
  int32_t n_diodes;
  int32_t l_pos;
  protobuf_c_boolean vflag;
  protobuf_c_boolean pflag;
  protobuf_c_boolean vconst;
  protobuf_c_boolean aconst;
};
#define SCF_EPIN__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_epin__descriptor) \
    , 0, 0, 0, 0, 0,NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }


struct  _ScfEconn
{
  ProtobufCMessage base;
  uint64_t lid;
  size_t n_cids;
  uint64_t *cids;
};
#define SCF_ECONN__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_econn__descriptor) \
    , 0, 0,NULL }


struct  _ScfEline
{
  ProtobufCMessage base;
  uint64_t id;
  size_t n_pins;
  uint64_t *pins;
  uint64_t c_pins;
  uint64_t flags;
  int64_t color;
  size_t n_conns;
  ScfEconn **conns;
  size_t n_lines;
  ScfLine **lines;
  double v;
  double jv;
  double a;
  double ja;
  protobuf_c_boolean vconst;
  protobuf_c_boolean aconst;
  protobuf_c_boolean vflag;
};
#define SCF_ELINE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_eline__descriptor) \
    , 0, 0,NULL, 0, 0, 0, 0,NULL, 0,NULL, 0, 0, 0, 0, 0, 0, 0 }


struct  _ScfEcomponent
{
  ProtobufCMessage base;
  uint64_t id;
  uint64_t type;
  uint64_t model;
  size_t n_pins;
  ScfEpin **pins;
  double v;
  double jv;
  double a;
  double ja;
  double r;
  double jr;
  double uf;
  double uh;
  int64_t color;
  int32_t status;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  protobuf_c_boolean vflag;
  protobuf_c_boolean lock;
};
#define SCF_ECOMPONENT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_ecomponent__descriptor) \
    , 0, 0, 0, 0,NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }


struct  _ScfEfunction
{
  ProtobufCMessage base;
  char *name;
  size_t n_components;
  ScfEcomponent **components;
  size_t n_elines;
  ScfEline **elines;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};
#define SCF_EFUNCTION__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_efunction__descriptor) \
    , NULL, 0,NULL, 0,NULL, 0, 0, 0, 0 }


struct  _ScfEboard
{
  ProtobufCMessage base;
  size_t n_functions;
  ScfEfunction **functions;
};
#define SCF_EBOARD__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&scf_eboard__descriptor) \
    , 0,NULL }


/* ScfLine methods */
void   scf_line__init
                     (ScfLine         *message);
size_t scf_line__get_packed_size
                     (const ScfLine   *message);
size_t scf_line__pack
                     (const ScfLine   *message,
                      uint8_t             *out);
size_t scf_line__pack_to_buffer
                     (const ScfLine   *message,
                      ProtobufCBuffer     *buffer);
ScfLine *
       scf_line__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_line__free_unpacked
                     (ScfLine *message,
                      ProtobufCAllocator *allocator);
/* ScfEpin methods */
void   scf_epin__init
                     (ScfEpin         *message);
size_t scf_epin__get_packed_size
                     (const ScfEpin   *message);
size_t scf_epin__pack
                     (const ScfEpin   *message,
                      uint8_t             *out);
size_t scf_epin__pack_to_buffer
                     (const ScfEpin   *message,
                      ProtobufCBuffer     *buffer);
ScfEpin *
       scf_epin__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_epin__free_unpacked
                     (ScfEpin *message,
                      ProtobufCAllocator *allocator);
/* ScfEconn methods */
void   scf_econn__init
                     (ScfEconn         *message);
size_t scf_econn__get_packed_size
                     (const ScfEconn   *message);
size_t scf_econn__pack
                     (const ScfEconn   *message,
                      uint8_t             *out);
size_t scf_econn__pack_to_buffer
                     (const ScfEconn   *message,
                      ProtobufCBuffer     *buffer);
ScfEconn *
       scf_econn__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_econn__free_unpacked
                     (ScfEconn *message,
                      ProtobufCAllocator *allocator);
/* ScfEline methods */
void   scf_eline__init
                     (ScfEline         *message);
size_t scf_eline__get_packed_size
                     (const ScfEline   *message);
size_t scf_eline__pack
                     (const ScfEline   *message,
                      uint8_t             *out);
size_t scf_eline__pack_to_buffer
                     (const ScfEline   *message,
                      ProtobufCBuffer     *buffer);
ScfEline *
       scf_eline__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_eline__free_unpacked
                     (ScfEline *message,
                      ProtobufCAllocator *allocator);
/* ScfEcomponent methods */
void   scf_ecomponent__init
                     (ScfEcomponent         *message);
size_t scf_ecomponent__get_packed_size
                     (const ScfEcomponent   *message);
size_t scf_ecomponent__pack
                     (const ScfEcomponent   *message,
                      uint8_t             *out);
size_t scf_ecomponent__pack_to_buffer
                     (const ScfEcomponent   *message,
                      ProtobufCBuffer     *buffer);
ScfEcomponent *
       scf_ecomponent__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_ecomponent__free_unpacked
                     (ScfEcomponent *message,
                      ProtobufCAllocator *allocator);
/* ScfEfunction methods */
void   scf_efunction__init
                     (ScfEfunction         *message);
size_t scf_efunction__get_packed_size
                     (const ScfEfunction   *message);
size_t scf_efunction__pack
                     (const ScfEfunction   *message,
                      uint8_t             *out);
size_t scf_efunction__pack_to_buffer
                     (const ScfEfunction   *message,
                      ProtobufCBuffer     *buffer);
ScfEfunction *
       scf_efunction__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_efunction__free_unpacked
                     (ScfEfunction *message,
                      ProtobufCAllocator *allocator);
/* ScfEboard methods */
void   scf_eboard__init
                     (ScfEboard         *message);
size_t scf_eboard__get_packed_size
                     (const ScfEboard   *message);
size_t scf_eboard__pack
                     (const ScfEboard   *message,
                      uint8_t             *out);
size_t scf_eboard__pack_to_buffer
                     (const ScfEboard   *message,
                      ProtobufCBuffer     *buffer);
ScfEboard *
       scf_eboard__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   scf_eboard__free_unpacked
                     (ScfEboard *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*ScfLine_Closure)
                 (const ScfLine *message,
                  void *closure_data);
typedef void (*ScfEpin_Closure)
                 (const ScfEpin *message,
                  void *closure_data);
typedef void (*ScfEconn_Closure)
                 (const ScfEconn *message,
                  void *closure_data);
typedef void (*ScfEline_Closure)
                 (const ScfEline *message,
                  void *closure_data);
typedef void (*ScfEcomponent_Closure)
                 (const ScfEcomponent *message,
                  void *closure_data);
typedef void (*ScfEfunction_Closure)
                 (const ScfEfunction *message,
                  void *closure_data);
typedef void (*ScfEboard_Closure)
                 (const ScfEboard *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor scf_line__descriptor;
extern const ProtobufCMessageDescriptor scf_epin__descriptor;
extern const ProtobufCMessageDescriptor scf_econn__descriptor;
extern const ProtobufCMessageDescriptor scf_eline__descriptor;
extern const ProtobufCMessageDescriptor scf_ecomponent__descriptor;
extern const ProtobufCMessageDescriptor scf_efunction__descriptor;
extern const ProtobufCMessageDescriptor scf_eboard__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_scf_5feda_2eproto__INCLUDED */
