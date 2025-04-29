// -*- C++ -*-
/*!
 *
 * THIS FILE IS GENERATED AUTOMATICALLY!! DO NOT EDIT!!
 *
 * @file DAQServiceSkel.h 
 * @brief DAQService server skeleton header wrapper code
 * @date Wed Jan 15 13:28:25 2025 
 *
 */

#ifndef DAQSERVICESKEL_H
#define DAQSERVICESKEL_H


#include <rtm/config_rtc.h>
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#if   defined ORB_IS_TAO
#  include "DAQServiceC.h"
#  include "DAQServiceS.h"
#elif defined ORB_IS_OMNIORB
#  if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    ifdef USE_stub_in_nt_dll
#        undef USE_stub_in_nt_dll
#    endif
#    ifdef OpenRTM_IMPORT_SYMBOL
#        define USE_stub_in_nt_dll
#    endif
#  endif
#  include "DAQService.hh"
//#  include "DAQServiceUtil.h"
#elif defined ORB_IS_MICO
#  include "DAQService.h"
#elif defined ORB_IS_ORBIT2
#  include "/DAQService-cpp-stubs.h"
#  include "/DAQService-cpp-skels.h"
#elif defined ORB_IS_RTORB
#  include "DAQService.h"
#elif defined ORB_IS_ORBEXPRESS
#  include "DAQService.h"
#else
#  error "NO ORB defined"
#endif

#endif // DAQSERVICESKEL_H
