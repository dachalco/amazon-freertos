/*
 * Amazon FreeRTOS PKCS#11 V1.0.8
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

/*------------------------------------------------------------------------------ */
/* */
/* This code was auto-generated by a tool. */
/* */
/* Changes to this file may cause incorrect behavior and will be */
/* lost if the code is regenerated. */
/* */
/*----------------------------------------------------------------------------- */

#include "iot_test_pkcs11_globals.h"

CK_MECHANISM generateValidSigningMechanism()
{
    switch( xMechanismType )
    {
        case CKM_RSA_PKCS:
        case CKM_ECDSA_SHA1:
        default:
            return ( CK_MECHANISM ) {
                       xMechanismType, NULL_PTR, 0
            };
    }
}

void generateRSAKeyPair( CK_OBJECT_HANDLE_PTR phPrivateKey,
                         CK_BBOOL doesSign,
                         CK_OBJECT_HANDLE_PTR phPublicKey,
                         CK_BBOOL doesVerify )
{
    CK_OBJECT_HANDLE hCertificate;

    prvProvisionRsaTestCredentials( phPrivateKey, &hCertificate );
}

void generateECDSAKeyPair( CK_OBJECT_HANDLE_PTR phPrivateKey,
                           CK_BBOOL doesSign,
                           CK_OBJECT_HANDLE_PTR phPublicKey,
                           CK_BBOOL doesVerify )
{
    CK_OBJECT_HANDLE hCertificate;

    prvProvisionCredentialsWithKeyImport( phPrivateKey, &hCertificate, phPublicKey );
}

void generateValidSingingKeyPair()
{
    switch( xMechanismType )
    {
        case CKM_RSA_PKCS:
            generateRSAKeyPair( &xPrivateKey, xCkTrue, &xPublicKey, xCkTrue );
            break;

        case CKM_ECDSA:
        default:
            generateECDSAKeyPair( &xPrivateKey, xCkTrue, &xPublicKey, xCkTrue );
            break;
    }
}

void C_SignInit_normal_behavior()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_OK, rv );
}

void C_SignInit_exceptional_behavior_0()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_OPERATION_ACTIVE, rv );
}

void C_SignInit_exceptional_behavior_1()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_OPERATION_ACTIVE, rv );
}

void C_SignInit_exceptional_behavior_2()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_MECHANISM_PTR pMechanism = NULL_PTR;
    CK_OBJECT_HANDLE hKey = xPrivateKey;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_ARGUMENTS_BAD, rv );
}

void C_SignInit_exceptional_behavior_3()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_SESSION_HANDLE_INVALID, rv );
}

void C_SignInit_exceptional_behavior_4()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_KEY_HANDLE_INVALID, rv );
}

void C_SignInit_exceptional_behavior_5()
{
    /* TODO: not yet implemented */
    return;

    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = 0;
    CK_OBJECT_HANDLE publicKey = 0;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_KEY_FUNCTION_NOT_PERMITTED, rv );
}

void C_SignInit_exceptional_behavior_6()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    if( xMechanismType == CKM_RSA_PKCS )
    {
        /* TODO: Not yet implemented */
        return;
    }
    else
    {
        hKey = xPublicKey;
    }

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_KEY_TYPE_INCONSISTENT, rv );
}

void C_SignInit_exceptional_behavior_7()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = ( CK_MECHANISM ) {
        CKM_AES_CBC, NULL_PTR, 0
    };
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_MECHANISM_INVALID, rv );
}

void C_SignInit_exceptional_behavior_8()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_CRYPTOKI_NOT_INITIALIZED, rv );
}

void C_SignInit_exceptional_behavior_9()
{
    CK_SESSION_HANDLE hSession = xGlobalSession;
    CK_OBJECT_HANDLE hKey = xPrivateKey;
    CK_MECHANISM pMechanism_val = generateValidSigningMechanism();
    CK_MECHANISM_PTR pMechanism = &pMechanism_val;

    CK_RV rv = pxGlobalFunctionList->C_SignInit( hSession, pMechanism, hKey );

    TEST_ASSERT_EQUAL( CKR_CRYPTOKI_NOT_INITIALIZED, rv );
}
