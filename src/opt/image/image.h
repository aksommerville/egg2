/* image.h
 * General image decoder.
 */
 
#ifndef IMAGE_H
#define IMAGE_H

/* Convenient API.
 * Just two calls: One to read the dimensions, and another to decode into your buffer.
 * Pixels are always 32-bit RGBA with the minimum stride.
 */
 
int image_measure(int *w,int *h,const void *src,int srcc);
int image_decode(void *dst,int dsta,const void *src,int srcc);

//TODO Detailed API.

#endif
