/*
 * FreeRTOS V202012.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#ifndef __AWS_CODESIGN_KEYS__H__
#define __AWS_CODESIGN_KEYS__H__

/*
 * PEM-encoded code signer certificate
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"
 * "...base64 data...\n"
 * "-----END CERTIFICATE-----\n";
 */
static const char signingcredentialSIGNING_CERTIFICATE_PEM[] = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIBYTCCAQagAwIBAgIUMkplVxVLCKsbuR9fKFzJUSlS0ScwCgYIKoZIzj0EAwIw\n" \
"HTEbMBkGA1UEAwwSZGNoYWxjb0BhbWF6b24uY29tMB4XDTIxMDUxOTIxNTA0M1oX\n" \
"DTIyMDUxOTIxNTA0M1owHTEbMBkGA1UEAwwSZGNoYWxjb0BhbWF6b24uY29tMFkw\n" \
"EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEKRZfxZWnFystcZFzwzNJWBavZ1dHAQs7\n" \
"M4Cr959J0RoT/2FMbg7Rb8+hsa5oPsO0AlRD02YDrRrCJ/C4VsOFl6MkMCIwCwYD\n" \
"VR0PBAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMDMAoGCCqGSM49BAMCA0kAMEYC\n" \
"IQCXYls24Uy+SH0zztY21FneTj5yW4/F17T2/04EebeZNgIhAOG798VC61JlftXN\n" \
"HoOBpe5rD38OqSoQSza3bm5O4F6M\n" \
"-----END CERTIFICATE-----\n";

#endif
