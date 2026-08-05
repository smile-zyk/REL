#ifndef REL_VALUE_API_H
#define REL_VALUE_API_H

// ---------------------------------------------------------------------------
//  rel_value DLL export / import
// ---------------------------------------------------------------------------
//  rel_value is a SHARED library that hosts the value-level operators for
//  REL (arithmetic, comparison, logical, bitwise, shift, pow) on top of the
//  xdataset storage types.  The symbols are defined inside namespace rel
//  so that argument-dependent lookup keeps working for expressions like
//  `m1 + m2` where m1/m2 are xdataset::Measurement.
#ifdef _WIN32
  #ifdef REL_VALUE_BUILD_DLL
    #define REL_VALUE_API __declspec(dllexport)
  #else
    #define REL_VALUE_API __declspec(dllimport)
  #endif
#else
  #define REL_VALUE_API
#endif

#endif // REL_VALUE_API_H
