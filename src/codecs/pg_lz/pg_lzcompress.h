/* ----------
 * pg_lzcompress.h -
 *
 *	Definitions for the builtin LZ compressor
 *
 * src/include/utils/pg_lzcompress.h
 * ----------
 */

#ifndef _PG_LZCOMPRESS_H_
#define _PG_LZCOMPRESS_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef int32_t  int32;
typedef uint32_t uint32;
typedef uint8_t  uint8;

 /*
  * These structs describe the header of a varlena object that may have been
  * TOASTed.  Generally, don't reference these structs directly, but use the
  * macros below.
  *
  * We use separate structs for the aligned and unaligned cases because the
  * compiler might otherwise think it could generate code that assumes
  * alignment while touching fields of a 1-byte-header varlena.
  */
 typedef union
 {
     struct                      /* Normal varlena (4-byte length) */
     {
         uint32      va_header;
         char        va_data[1];
     }           va_4byte;
     struct                      /* Compressed-in-line format */
     {
         uint32      va_header;
         uint32      va_rawsize; /* Original data size (excludes header) */
         char        va_data[1]; /* Compressed data */
     }           va_compressed;
 } varattrib_4b;

 typedef struct
 {
     uint8       va_header;
     char        va_data[1];     /* Data begins here */
 } varattrib_1b;

 typedef struct
 {
     uint8       va_header;      /* Always 0x80 or 0x01 */
     uint8       va_len_1be;     /* Physical length of datum */
     char        va_data[1];     /* Data (for now always a TOAST pointer) */
 } varattrib_1b_e;

 /*
  * Bit layouts for varlena headers on big-endian machines:
  *
  * 00xxxxxx 4-byte length word, aligned, uncompressed data (up to 1G)
  * 01xxxxxx 4-byte length word, aligned, *compressed* data (up to 1G)
  * 10000000 1-byte length word, unaligned, TOAST pointer
  * 1xxxxxxx 1-byte length word, unaligned, uncompressed data (up to 126b)
  *
  * Bit layouts for varlena headers on little-endian machines:
  *
  * xxxxxx00 4-byte length word, aligned, uncompressed data (up to 1G)
  * xxxxxx10 4-byte length word, aligned, *compressed* data (up to 1G)
  * 00000001 1-byte length word, unaligned, TOAST pointer
  * xxxxxxx1 1-byte length word, unaligned, uncompressed data (up to 126b)
  *
  * The "xxx" bits are the length field (which includes itself in all cases).
  * In the big-endian case we mask to extract the length, in the little-endian
  * case we shift.  Note that in both cases the flag bits are in the physically
  * first byte.  Also, it is not possible for a 1-byte length word to be zero;
  * this lets us disambiguate alignment padding bytes from the start of an
  * unaligned datum.  (We now *require* pad bytes to be filled with zero!)
  */

 /*
  * Endian-dependent macros.  These are considered internal --- use the
  * external macros below instead of using these directly.
  *
  * Note: IS_1B is true for external toast records but VARSIZE_1B will return 0
  * for such records. Hence you should usually check for IS_EXTERNAL before
  * checking for IS_1B.
  */

 #ifdef WORDS_BIGENDIAN

 #define VARATT_IS_4B(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x80) == 0x00)
 #define VARATT_IS_4B_U(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0xC0) == 0x00)
 #define VARATT_IS_4B_C(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0xC0) == 0x40)
 #define VARATT_IS_1B(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x80) == 0x80)
 #define VARATT_IS_1B_E(PTR) \
     ((((varattrib_1b *) (PTR))->va_header) == 0x80)
 #define VARATT_NOT_PAD_BYTE(PTR) \
     (*((uint8 *) (PTR)) != 0)

 /* VARSIZE_4B() should only be used on known-aligned data */
 #define VARSIZE_4B(PTR) \
     (((varattrib_4b *) (PTR))->va_4byte.va_header & 0x3FFFFFFF)
 #define VARSIZE_1B(PTR) \
     (((varattrib_1b *) (PTR))->va_header & 0x7F)
 #define VARSIZE_1B_E(PTR) \
     (((varattrib_1b_e *) (PTR))->va_len_1be)

 #define SET_VARSIZE_4B(PTR,len) \
     (((varattrib_4b *) (PTR))->va_4byte.va_header = (len) & 0x3FFFFFFF)
 #define SET_VARSIZE_4B_C(PTR,len) \
     (((varattrib_4b *) (PTR))->va_4byte.va_header = ((len) & 0x3FFFFFFF) | 0x40000000)
 #define SET_VARSIZE_1B(PTR,len) \
     (((varattrib_1b *) (PTR))->va_header = (len) | 0x80)
 #define SET_VARSIZE_1B_E(PTR,len) \
     (((varattrib_1b_e *) (PTR))->va_header = 0x80, \
      ((varattrib_1b_e *) (PTR))->va_len_1be = (len))
 #else                           /* !WORDS_BIGENDIAN */

 #define VARATT_IS_4B(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x01) == 0x00)
 #define VARATT_IS_4B_U(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x03) == 0x00)
 #define VARATT_IS_4B_C(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x03) == 0x02)
 #define VARATT_IS_1B(PTR) \
     ((((varattrib_1b *) (PTR))->va_header & 0x01) == 0x01)
 #define VARATT_IS_1B_E(PTR) \
     ((((varattrib_1b *) (PTR))->va_header) == 0x01)
 #define VARATT_NOT_PAD_BYTE(PTR) \
     (*((uint8 *) (PTR)) != 0)

 /* VARSIZE_4B() should only be used on known-aligned data */
 #define VARSIZE_4B(PTR) \
     ((((varattrib_4b *) (PTR))->va_4byte.va_header >> 2) & 0x3FFFFFFF)
 #define VARSIZE_1B(PTR) \
     ((((varattrib_1b *) (PTR))->va_header >> 1) & 0x7F)
 #define VARSIZE_1B_E(PTR) \
     (((varattrib_1b_e *) (PTR))->va_len_1be)

 #define SET_VARSIZE_4B(PTR,len) \
     (((varattrib_4b *) (PTR))->va_4byte.va_header = (((uint32) (len)) << 2))
 #define SET_VARSIZE_4B_C(PTR,len) \
     (((varattrib_4b *) (PTR))->va_4byte.va_header = (((uint32) (len)) << 2) | 0x02)
 #define SET_VARSIZE_1B(PTR,len) \
     (((varattrib_1b *) (PTR))->va_header = (((uint8) (len)) << 1) | 0x01)
 #define SET_VARSIZE_1B_E(PTR,len) \
     (((varattrib_1b_e *) (PTR))->va_header = 0x01, \
      ((varattrib_1b_e *) (PTR))->va_len_1be = (len))
 #endif   /* WORDS_BIGENDIAN */

#define VARHDRSZ_SHORT          1
#define VARATT_SHORT_MAX        0x7F
#define VARATT_CAN_MAKE_SHORT(PTR) \
    (VARATT_IS_4B_U(PTR) && \
     (VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT) <= VARATT_SHORT_MAX)
#define VARATT_CONVERTED_SHORT_SIZE(PTR) \
    (VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT)

#define VARHDRSZ_EXTERNAL       2

#define VARDATA_4B(PTR)     (((varattrib_4b *) (PTR))->va_4byte.va_data)
#define VARDATA_4B_C(PTR)   (((varattrib_4b *) (PTR))->va_compressed.va_data)
#define VARDATA_1B(PTR)     (((varattrib_1b *) (PTR))->va_data)
#define VARDATA_1B_E(PTR)   (((varattrib_1b_e *) (PTR))->va_data)

#define VARRAWSIZE_4B_C(PTR) \
    (((varattrib_4b *) (PTR))->va_compressed.va_rawsize)

/* Externally visible macros */

/*
 * VARDATA, VARSIZE, and SET_VARSIZE are the recommended API for most code
 * for varlena datatypes.  Note that they only work on untoasted,
 * 4-byte-header Datums!
 *
 * Code that wants to use 1-byte-header values without detoasting should
 * use VARSIZE_ANY/VARSIZE_ANY_EXHDR/VARDATA_ANY.  The other macros here
 * should usually be used only by tuple assembly/disassembly code and
 * code that specifically wants to work with still-toasted Datums.
 *
 * WARNING: It is only safe to use VARDATA_ANY() -- typically with
 * PG_DETOAST_DATUM_PACKED() -- if you really don't care about the alignment.
 * Either because you're working with something like text where the alignment
 * doesn't matter or because you're not going to access its constituent parts
 * and just use things like memcpy on it anyways.
 */
#define VARDATA(PTR)                        VARDATA_4B(PTR)
#define VARSIZE(PTR)                        VARSIZE_4B(PTR)

#define VARSIZE_SHORT(PTR)                  VARSIZE_1B(PTR)
#define VARDATA_SHORT(PTR)                  VARDATA_1B(PTR)

#define VARSIZE_EXTERNAL(PTR)               VARSIZE_1B_E(PTR)
#define VARDATA_EXTERNAL(PTR)               VARDATA_1B_E(PTR)

#define VARATT_IS_COMPRESSED(PTR)           VARATT_IS_4B_C(PTR)
#define VARATT_IS_EXTERNAL(PTR)             VARATT_IS_1B_E(PTR)
#define VARATT_IS_SHORT(PTR)                VARATT_IS_1B(PTR)
#define VARATT_IS_EXTENDED(PTR)             (!VARATT_IS_4B_U(PTR))

#define SET_VARSIZE(PTR, len)               SET_VARSIZE_4B(PTR, len)
#define SET_VARSIZE_SHORT(PTR, len)         SET_VARSIZE_1B(PTR, len)
#define SET_VARSIZE_COMPRESSED(PTR, len)    SET_VARSIZE_4B_C(PTR, len)
#define SET_VARSIZE_EXTERNAL(PTR, len)      SET_VARSIZE_1B_E(PTR, len)

#define VARSIZE_ANY(PTR) \
    (VARATT_IS_1B_E(PTR) ? VARSIZE_1B_E(PTR) : \
     (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR) : \
      VARSIZE_4B(PTR)))

#define VARSIZE_ANY_EXHDR(PTR) \
    (VARATT_IS_1B_E(PTR) ? VARSIZE_1B_E(PTR)-VARHDRSZ_EXTERNAL : \
     (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR)-VARHDRSZ_SHORT : \
      VARSIZE_4B(PTR)-VARHDRSZ))

/* caution: this will not work on an external or compressed-in-line Datum */
/* caution: this will return a possibly unaligned pointer */
#define VARDATA_ANY(PTR) \
     (VARATT_IS_1B(PTR) ? VARDATA_1B(PTR) : VARDATA_4B(PTR))


/* ----------
 * PGLZ_Header -
 *
 *		The information at the start of the compressed data.
 * ----------
 */
typedef struct PGLZ_Header
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	int32		rawsize;
} PGLZ_Header;


/* ----------
 * PGLZ_MAX_OUTPUT -
 *
 *		Macro to compute the buffer size required by pglz_compress().
 *		We allow 4 bytes for overrun before detecting compression failure.
 * ----------
 */
#define PGLZ_MAX_OUTPUT(_dlen)			((_dlen) + 4 + sizeof(PGLZ_Header))

/* ----------
 * PGLZ_RAW_SIZE -
 *
 *		Macro to determine the uncompressed data size contained
 *		in the entry.
 * ----------
 */
#define PGLZ_RAW_SIZE(_lzdata)			((_lzdata)->rawsize)


/* ----------
 * PGLZ_Strategy -
 *
 *		Some values that control the compression algorithm.
 *
 *		min_input_size		Minimum input data size to consider compression.
 *
 *		max_input_size		Maximum input data size to consider compression.
 *
 *		min_comp_rate		Minimum compression rate (0-99%) to require.
 *							Regardless of min_comp_rate, the output must be
 *							smaller than the input, else we don't store
 *							compressed.
 *
 *		first_success_by	Abandon compression if we find no compressible
 *							data within the first this-many bytes.
 *
 *		match_size_good		The initial GOOD match size when starting history
 *							lookup. When looking up the history to find a
 *							match that could be expressed as a tag, the
 *							algorithm does not always walk back entirely.
 *							A good match fast is usually better than the
 *							best possible one very late. For each iteration
 *							in the lookup, this value is lowered so the
 *							longer the lookup takes, the smaller matches
 *							are considered good.
 *
 *		match_size_drop		The percentage by which match_size_good is lowered
 *							after each history check. Allowed values are
 *							0 (no change until end) to 100 (only check
 *							latest history entry at all).
 * ----------
 */
typedef struct PGLZ_Strategy
{
	int32		min_input_size;
	int32		max_input_size;
	int32		min_comp_rate;
	int32		first_success_by;
	int32		match_size_good;
	int32		match_size_drop;
} PGLZ_Strategy;


/* ----------
 * The standard strategies
 *
 *		PGLZ_strategy_default		Recommended default strategy for TOAST.
 *
 *		PGLZ_strategy_always		Try to compress inputs of any length.
 *									Fallback to uncompressed storage only if
 *									output would be larger than input.
 * ----------
 */
extern const PGLZ_Strategy *const PGLZ_strategy_default;
extern const PGLZ_Strategy *const PGLZ_strategy_always;


/* ----------
 * Global function declarations
 * ----------
 */
extern bool pglz_compress(const char *source, int32 slen, PGLZ_Header *dest,
			  const PGLZ_Strategy *strategy);
extern bool pglz_decompress(const PGLZ_Header *source, char *dest);

#endif   /* _PG_LZCOMPRESS_H_ */
