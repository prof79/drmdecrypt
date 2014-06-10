/* drmdecrypt -- DRM decrypting tool for Samsung TVs
 *
 * Copyright (C) 2014 - Bernhard Froehlich <decke@bluelife.at>
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the GPL v2 license.  See the LICENSE file for details.
 */

enum {
   TRC_DEBUG = 0,
   TRC_INFO,   
   TRC_WARN,
   TRC_ERROR
};

static int tracelevel = TRC_WARN;

#define trace(L, M, ...) \
   if(L >= tracelevel) { \
      if (tracelevel == 0) { \
         fprintf(stderr, "%s %s:%d: " M "\n", L == 0 ? "DEBUG" : L == 1 ? "INFO" : L == 2 ? "WARN" : "ERROR", __FILE__, __LINE__, ##__VA_ARGS__); \
      } \
      else { \
         fprintf(stderr, "%s " M "\n", L == 0 ? "DEBUG" : L == 1 ? "INFO" : L == 2 ? "WARN" : "ERROR", ##__VA_ARGS__); \
      } \
   }

