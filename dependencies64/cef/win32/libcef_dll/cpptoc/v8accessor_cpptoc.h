// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=74fe0a1d5896cd334f9982e97daad1b147ede77d$
//

#ifndef CEF_LIBCEF_DLL_CPPTOC_V8ACCESSOR_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_V8ACCESSOR_CPPTOC_H_
#pragma once

#if !defined(WRAPPING_CEF_SHARED)
#error This file can be included wrapper-side only
#endif

#include "include/capi/cef_v8_capi.h"
#include "include/cef_v8.h"
#include "libcef_dll/cpptoc/cpptoc_ref_counted.h"

// Wrap a C++ class with a C structure.
// This class may be instantiated and accessed wrapper-side only.
class CefV8AccessorCppToC : public CefCppToCRefCounted<CefV8AccessorCppToC,
                                                       CefV8Accessor,
                                                       cef_v8accessor_t> {
 public:
  CefV8AccessorCppToC();
};

#endif  // CEF_LIBCEF_DLL_CPPTOC_V8ACCESSOR_CPPTOC_H_
