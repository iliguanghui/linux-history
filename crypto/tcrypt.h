/* 
 * Quick & dirty crypto testing module.
 *
 * This will only exist until we have a better testing mechanism
 * (e.g. a char device).
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2002 Jean-Francois Dive <jef@linuxbe.org>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */
#ifndef _CRYPTO_TCRYPT_H
#define _CRYPTO_TCRYPT_H

#define MD5_DIGEST_SIZE		16
#define MD4_DIGEST_SIZE		16
#define SHA1_DIGEST_SIZE	20

/*
 * MD4 test vectors from RFC1320
 */
#define MD4_TEST_VECTORS	7

struct md4_testvec {
	char plaintext[128];
	char digest[MD4_DIGEST_SIZE];
} md4_tv_template[] = {
	{ "",
		{ 0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31,
		  0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0 }
	},
	
	{ "a",
		{ 0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46,
		  0x24, 0x5e, 0x05, 0xfb, 0xdb, 0xd6, 0xfb, 0x24 }
	},

	{ "abc",
		{ 0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52,
		  0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d }
	},
	
	{ "message digest",
		{ 0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8,
		  0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b }
	},
	
	{ "abcdefghijklmnopqrstuvwxyz",
		{ 0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd,
		  0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9 }
	},
		
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		{ 0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35,
		  0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4 }
	},
	
	{ "123456789012345678901234567890123456789012345678901234567890123"
	  "45678901234567890",
		{ 0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19,
		  0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36 }
	},
};

/*
 * MD5 test vectors from RFC1321
 */
#define MD5_TEST_VECTORS	7

struct md5_testvec {
	char plaintext[128];
	char digest[MD5_DIGEST_SIZE];
} md5_tv_template[] = {
	{ "",
		{ 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
		  0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e } },
	        	
	{ "a",
		{ 0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8,
		  0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61 } },
		  
	{ "abc",
		{ 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
		  0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72 } }, 
		  
	{ "message digest",
		{ 0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d,
		  0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0 } },
		  
	{ "abcdefghijklmnopqrstuvwxyz",
		{ 0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00,
		  0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b } },
		  
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		{ 0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5,
		  0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f } },
		  
	{ "12345678901234567890123456789012345678901234567890123456789012"
	  "345678901234567890",
		{ 0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55,
		  0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a } }
};

/*
 * HMAC-MD5 test vectors from RFC2202
 * (These need to be fixed to not use strlen).
 */
#define HMAC_MD5_TEST_VECTORS	7

struct hmac_md5_testvec {
	char key[128];
	char plaintext[128];
	char digest[MD5_DIGEST_SIZE];
};

struct hmac_md5_testvec hmac_md5_tv_template[] =
{
	
	{
		{ 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x00},
		  
		"Hi There",
		
		{ 0x92, 0x94, 0x72, 0x7a, 0x36, 0x38, 0xbb, 0x1c,
		  0x13, 0xf4, 0x8e, 0xf8, 0x15, 0x8b, 0xfc, 0x9d }
	},
	    
	{
		{ 'J', 'e', 'f', 'e', 0 },
		
		"what do ya want for nothing?",
		
		{ 0x75, 0x0c, 0x78, 0x3e, 0x6a, 0xb0, 0xb5, 0x03,
		  0xea, 0xa8, 0x6e, 0x31, 0x0a, 0x5d, 0xb7, 0x38 }
	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x00 },
		  
		{ 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0x00 },
		  
		{ 0x56, 0xbe, 0x34, 0x52, 0x1d, 0x14, 0x4c, 0x88,
		  0xdb, 0xb8, 0xc7, 0x33, 0xf0, 0xe8, 0xb3, 0xf6 }
	},
	
	{
		{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 
		  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00 },
		  
		{ 
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0x00 },
		  
		{ 0x69, 0x7e, 0xaf, 0x0a, 0xca, 0x3a, 0x3a, 0xea,
		  0x3a, 0x75, 0x16, 0x47, 0x46, 0xff, 0xaa, 0x79 }
	},
	
	{
		{ 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
		  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x00 },
		  
		"Test With Truncation",
		
		{ 0x56, 0x46, 0x1e, 0xf2, 0x34, 0x2e, 0xdc, 0x00,
		  0xf9, 0xba, 0xb9, 0x95, 0x69, 0x0e, 0xfd, 0x4c }
	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0x00 },
		  
		"Test Using Larger Than Block-Size Key - Hash Key First",
		
		{ 0x6b, 0x1a, 0xb7, 0xfe, 0x4b, 0xd7, 0xbf, 0x8f,
		  0x0b, 0x62, 0xe6, 0xce, 0x61, 0xb9, 0xd0, 0xcd }
	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0x00 },
		
		"Test Using Larger Than Block-Size Key and Larger Than One "
		"Block-Size Data",
		
		{ 0x6f, 0x63, 0x0f, 0xad, 0x67, 0xcd, 0xa0, 0xee,
		  0x1f, 0xb1, 0xf5, 0x62, 0xdb, 0x3a, 0xa5, 0x3e }
	}
};

	
/*
 * HMAC-SHA1 test vectors from RFC2202
 */

#define HMAC_SHA1_TEST_VECTORS	7

struct hmac_sha1_testvec {
	char key[128];
	char plaintext[128];
	char digest[SHA1_DIGEST_SIZE];
} hmac_sha1_tv_template[] = {
	
	{
		{ 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		  0x00},
		  
		"Hi There",

		{ 0xb6, 0x17, 0x31, 0x86, 0x55, 0x05, 0x72, 0x64,
                  0xe2, 0x8b, 0xc0, 0xb6, 0xfb ,0x37, 0x8c, 0x8e, 0xf1,
                  0x46, 0xbe, 0x00 }
	},
	    
	{
		{ 'J', 'e', 'f', 'e', 0 },
		
		"what do ya want for nothing?",

		{ 0xef, 0xfc, 0xdf, 0x6a, 0xe5, 0xeb, 0x2f, 0xa2, 0xd2, 0x74, 
		  0x16, 0xd5, 0xf1, 0x84, 0xdf, 0x9c, 0x25, 0x9a, 0x7c, 0x79 }

	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0x00},

		  
		{ 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
		  0x00 },
		  
		{ 0x12, 0x5d, 0x73, 0x42, 0xb9, 0xac, 0x11, 0xcd, 0x91, 0xa3, 
		  0x9a, 0xf4, 0x8a, 0xa1, 0x7b, 0x4f, 0x63, 0xf1, 0x75, 0xd3 }

	},
	
	{
		{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 
		  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00 },
		  
		{ 
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		  0x00 },
		  
		{ 0x4c, 0x90, 0x07, 0xf4, 0x02, 0x62, 0x50, 0xc6, 0xbc, 0x84, 
		  0x14, 0xf9, 0xbf, 0x50, 0xc8, 0x6c, 0x2d, 0x72, 0x35, 0xda }

	},
	
	{
		{ 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
		  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
		  0x00 },
		  
		"Test With Truncation",
		
		{ 0x4c, 0x1a, 0x03, 0x42, 0x4b, 0x55, 0xe0, 0x7f, 0xe7, 0xf2, 
  		  0x7b, 0xe1, 0xd5, 0x8b, 0xb9, 0x32, 0x4a, 0x9a, 0x5a, 0x04 }

	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0x00 },
		  
		"Test Using Larger Than Block-Size Key - Hash Key First",
		
		{ 0xaa, 0x4a, 0xe5, 0xe1, 0x52, 0x72, 0xd0, 0x0e, 0x95, 0x70, 
  		  0x56, 0x37, 0xce, 0x8a, 0x3b, 0x55, 0xed, 0x40, 0x21, 0x12 }

	},
	
	{
		{ 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		  0x00 },
		
		"Test Using Larger Than Block-Size Key and Larger Than One "
		"Block-Size Data",
		
		{ 0xe8, 0xe9, 0x9d, 0x0f, 0x45, 0x23, 0x7d, 0x78, 0x6d, 0x6b, 
		  0xba, 0xa7, 0x96, 0x5c, 0x78, 0x08, 0xbb, 0xff, 0x1a, 0x91 }
	}
};


/*
 * SHA1 test vectors  from from FIPS PUB 180-1
 */
#define SHA1_TEST_VECTORS	2

struct sha1_testvec {
	char plaintext[128];
	char digest[SHA1_DIGEST_SIZE];
} sha1_tv_template[] = {
	{ "abc",
	  { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E, 
	    0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C ,0x9C, 0xD0, 0xD8, 0x9D }
	},
	        	
	{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",

	  { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E ,0xBA, 0xAE,
	    0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1 }
	}
};

/*
 * DES test vectors (also need to test for weak keys etc).
 */
#define DES_ENC_TEST_VECTORS		5
#define DES_DEC_TEST_VECTORS		2
#define DES_CBC_ENC_TEST_VECTORS	4
#define DES_CBC_DEC_TEST_VECTORS	3
#define DES3_EDE_ENC_TEST_VECTORS	3
#define DES3_EDE_DEC_TEST_VECTORS	3

struct des_tv {
	unsigned int len;
	int fail;
	char key[24];
	char iv[8];
	char plaintext[128];
	char result[128];
};

struct des_tv des_enc_tv_template[] = {

	/* From Applied Cryptography */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0 },
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7 },
		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d }
	},
	
	/* Same key, different plaintext block */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0 },
		{ 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 },
		{ 0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b }
	},
	
	/* Sbox test from NBS */
	{
		8, 0,
		
		{ 0x7C, 0xA1, 0x10, 0x45, 0x4A, 0x1A, 0x6E, 0x57 },
		{ 0 },
		{ 0x01, 0xA1, 0xD6, 0xD0, 0x39, 0x77, 0x67, 0x42 },
		{ 0x69, 0x0F, 0x5B, 0x0D, 0x9A, 0x26, 0x93, 0x9B }
	},
	
	/* Three blocks */
	{
		24, 0,
		
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		
		{ 0 },
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7,
		  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		  0xca, 0xfe, 0xba, 0xbe, 0xfe, 0xed, 0xbe, 0xef },

		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d,
		  0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b,
		  0xb4, 0x99, 0x26, 0xf7, 0x1f, 0xe1, 0xd4, 0x90 },		  
	},
	
	/* Weak key */
	{
		8, 1,
		
		{ 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
		{ 0 },
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7 },
		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d }
	},
	
	/* Two blocks -- for testing encryption across pages */
	{
		16, 0,
		
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		
		{ 0 },
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7,
		  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 },

		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d,
		  0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b }
	},

	/* Two blocks -- for testing decryption across pages */
	{
		16, 0,
		
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		
		{ 0 },
		
		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d,
		  0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b },
		  
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7,
		  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 },
	},
	
	/* Four blocks -- for testing encryption with chunking */
	{
		24, 0,
		
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		
		{ 0 },
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7,
		  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		  0xca, 0xfe, 0xba, 0xbe, 0xfe, 0xed, 0xbe, 0xef,
		  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 },

		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d,
		  0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b,
		  0xb4, 0x99, 0x26, 0xf7, 0x1f, 0xe1, 0xd4, 0x90,
		  0xf7, 0x9c, 0x89, 0x2a, 0x33, 0x8f, 0x4a, 0x8b },
	},
	
};

struct des_tv des_dec_tv_template[] = {

	/* From Applied Cryptography */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0 },
		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d },
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7 },
	},
	
	/* Sbox test from NBS */
	{
		8, 0,

		{ 0x7C, 0xA1, 0x10, 0x45, 0x4A, 0x1A, 0x6E, 0x57 },
		{ 0 },
		{ 0x69, 0x0F, 0x5B, 0x0D, 0x9A, 0x26, 0x93, 0x9B },
		{ 0x01, 0xA1, 0xD6, 0xD0, 0x39, 0x77, 0x67, 0x42 }
	},
	
	/* Two blocks, for chunking test */
	{
		16, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0 },
		
		{ 0xc9, 0x57, 0x44, 0x25, 0x6a, 0x5e, 0xd3, 0x1d,
		  0x69, 0x0F, 0x5B, 0x0D, 0x9A, 0x26, 0x93, 0x9B },
		  
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xe7,
		  0xa3, 0x99, 0x7b, 0xca, 0xaf, 0x69, 0xa0, 0xf5 }
	},

};

struct des_tv des_cbc_enc_tv_template[] = {
	/* From OpenSSL */
	{
		24, 0,
		
		{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef},
		{0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10},
		
		{ 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x20, 
		  0x4E, 0x6F, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74, 
		  0x68, 0x65, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x20, 
		  0x66, 0x6F, 0x72, 0x20, 0x00, 0x31, 0x00, 0x00 },
		  
		{ 0xcc, 0xd1, 0x73, 0xff, 0xab, 0x20, 0x39, 0xf4, 
		  0xac, 0xd8, 0xae, 0xfd, 0xdf, 0xd8, 0xa1, 0xeb, 
		  0x46, 0x8e, 0x91, 0x15, 0x78, 0x88, 0xba, 0x68, 
		  0x1d, 0x26, 0x93, 0x97, 0xf7, 0xfe, 0x62, 0xb4 }
	},

	/* FIPS Pub 81 */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef },
		{ 0x4e, 0x6f, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74 },
		{ 0xe5, 0xc7, 0xcd, 0xde, 0x87, 0x2b, 0xf2, 0x7c },
		
	},
	
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0xe5, 0xc7, 0xcd, 0xde, 0x87, 0x2b, 0xf2, 0x7c },
		{ 0x68, 0x65, 0x20, 0x74, 0x69, 0x6d, 0x65, 0x20 },
		{ 0x43, 0xe9, 0x34, 0x00, 0x8c, 0x38, 0x9c, 0x0f },
	},
	
	{	
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0x43, 0xe9, 0x34, 0x00, 0x8c, 0x38, 0x9c, 0x0f },
		{ 0x66, 0x6f, 0x72, 0x20, 0x61, 0x6c, 0x6c, 0x20 },
		{ 0x68, 0x37, 0x88, 0x49, 0x9a, 0x7c, 0x05, 0xf6 },
	},
	
	/* Copy of openssl vector for chunk testing */
	
	/* From OpenSSL */
	{
		24, 0,
		
		{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef},
		{0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10},
		
		{ 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x20, 
		  0x4E, 0x6F, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74, 
		  0x68, 0x65, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x20, 
		  0x66, 0x6F, 0x72, 0x20, 0x00, 0x31, 0x00, 0x00 },
		  
		{ 0xcc, 0xd1, 0x73, 0xff, 0xab, 0x20, 0x39, 0xf4, 
		  0xac, 0xd8, 0xae, 0xfd, 0xdf, 0xd8, 0xa1, 0xeb, 
		  0x46, 0x8e, 0x91, 0x15, 0x78, 0x88, 0xba, 0x68, 
		  0x1d, 0x26, 0x93, 0x97, 0xf7, 0xfe, 0x62, 0xb4 }
	},

	
};

struct des_tv des_cbc_dec_tv_template[] = {

	/* FIPS Pub 81 */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef },
		{ 0xe5, 0xc7, 0xcd, 0xde, 0x87, 0x2b, 0xf2, 0x7c },
		{ 0x4e, 0x6f, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74 },
	},
	
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0xe5, 0xc7, 0xcd, 0xde, 0x87, 0x2b, 0xf2, 0x7c },
		{ 0x43, 0xe9, 0x34, 0x00, 0x8c, 0x38, 0x9c, 0x0f },
		{ 0x68, 0x65, 0x20, 0x74, 0x69, 0x6d, 0x65, 0x20 }, 
	},
	
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0x43, 0xe9, 0x34, 0x00, 0x8c, 0x38, 0x9c, 0x0f },
		{ 0x68, 0x37, 0x88, 0x49, 0x9a, 0x7c, 0x05, 0xf6 }, 
		{ 0x66, 0x6f, 0x72, 0x20, 0x61, 0x6c, 0x6c, 0x20 },
	},
	
	/* Copy of above, for chunk testing */
	
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef },
		{ 0x43, 0xe9, 0x34, 0x00, 0x8c, 0x38, 0x9c, 0x0f },
		{ 0x68, 0x37, 0x88, 0x49, 0x9a, 0x7c, 0x05, 0xf6 }, 
		{ 0x66, 0x6f, 0x72, 0x20, 0x61, 0x6c, 0x6c, 0x20 },
	},
};

/*
 * We really need some more test vectors, especially for DES3 CBC.
 */
struct des_tv des3_ede_enc_tv_template[] = {

	/* These are from openssl */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
		  0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
		  0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},
		  
		{ 0 },
		
		{ 0x73, 0x6F, 0x6D, 0x65, 0x64, 0x61, 0x74, 0x61  },
		
		{ 0x18, 0xd7, 0x48, 0xe5, 0x63, 0x62, 0x05, 0x72 },
	},
	
	{
		8, 0,
		
		{ 0x03,0x52,0x02,0x07,0x67,0x20,0x82,0x17,
		  0x86,0x02,0x87,0x66,0x59,0x08,0x21,0x98,
		  0x64,0x05,0x6A,0xBD,0xFE,0xA9,0x34,0x57  },
		 
		{ 0 },
		
		{ 0x73,0x71,0x75,0x69,0x67,0x67,0x6C,0x65  },
		
		{ 0xc0,0x7d,0x2a,0x0f,0xa5,0x66,0xfa,0x30  }
	},
	

	{
		8, 0,
		
		{ 0x10,0x46,0x10,0x34,0x89,0x98,0x80,0x20,
		  0x91,0x07,0xD0,0x15,0x89,0x19,0x01,0x01,
		  0x19,0x07,0x92,0x10,0x98,0x1A,0x01,0x01  },
		  
		{ 0 },
			  
		{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00  },
		
		{ 0xe1,0xef,0x62,0xc3,0x32,0xfe,0x82,0x5b  }	
	},
};

struct des_tv des3_ede_dec_tv_template[] = {

	/* These are from openssl */
	{
		8, 0,
		
		{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
		  0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
		  0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},
		  
		{ 0 },
		
		
		{ 0x18, 0xd7, 0x48, 0xe5, 0x63, 0x62, 0x05, 0x72 },
		
		{ 0x73, 0x6F, 0x6D, 0x65, 0x64, 0x61, 0x74, 0x61  },
	},
	
	{
		8, 0,
		
		{ 0x03,0x52,0x02,0x07,0x67,0x20,0x82,0x17,
		  0x86,0x02,0x87,0x66,0x59,0x08,0x21,0x98,
		  0x64,0x05,0x6A,0xBD,0xFE,0xA9,0x34,0x57  },
		 
		{ 0 },
		
		{ 0xc0,0x7d,0x2a,0x0f,0xa5,0x66,0xfa,0x30  },
		
		{ 0x73,0x71,0x75,0x69,0x67,0x67,0x6C,0x65  },
		
	},
	

	{
		8, 0,
		
		{ 0x10,0x46,0x10,0x34,0x89,0x98,0x80,0x20,
		  0x91,0x07,0xD0,0x15,0x89,0x19,0x01,0x01,
		  0x19,0x07,0x92,0x10,0x98,0x1A,0x01,0x01  },
		  
		{ 0 },
			  
		{ 0xe1,0xef,0x62,0xc3,0x32,0xfe,0x82,0x5b  },
		
		{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00  },
	},
};
	
#endif	/* _CRYPTO_TCRYPT_H */

