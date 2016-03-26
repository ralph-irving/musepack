#ifndef FILEIO_ENDIAN_H
#define FILEIO_ENDIAN_H

void    (*Calc_EndianSwapper (endian_t endianess, unsigned int bits) ) (void *, size_t);
uint8_t*        Write_to_80bit_BE_IEEE854_Float  ( uint8_t*       p, long double  val );
const uint8_t*  Read_from_80bit_BE_IEEE854_Float ( const uint8_t* p, long double* val );

#endif /* FILEIO_ENDIAN_H */
