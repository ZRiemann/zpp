#pragma once

#ifndef NSB_ZPP
#define NSB_ZPP namespace z{
#define NSE_ZPP }
#define USE_ZPP using namespace z;
#endif

// moodycamel wrapers
#ifndef NSB_CAMEL
#define NSB_CAMEL NSB_ZPP namespace camel{
#define NSE_CAMEL NSE_ZPP }
#define USE_CAMEL using namespace z::camel;
#endif

#ifndef NSB_FOLLY
#define NSB_FOLLY NSB_ZPP namespace fo{
#define NSE_FOLLY NSE_ZPP }
#define USE_FOLLY using namespace z::fo;
#endif

#ifndef NSB_TASKFLOW
#define NSB_TASKFLOW NSB_ZPP namespace ztf{
#define NSE_TASKFLOW NSE_ZPP }
#define USE_TASKFLOW using namespace z::ztf;
#endif

#ifndef NSB_HPX
#define NSB_HPX NSB_ZPP namespace zhpx{
#define NSE_HPX NSE_ZPP }
#define USE_HPX using namespace z::zhpx;
#endif

#ifndef NSB_CUDA
#define NSB_CUDA NSB_ZPP namespace cu{
#define NSE_CUDA NSE_ZPP }
#define USE_CUDA using namespace z::cu;
#endif

#ifndef NSB_STATIC
#define NSB_STATIC namespace{
#define NSE_STATIC }
#endif

// application namespace
#ifndef NSB_APP
#define NSB_APP NSB_ZPP namespace app{
#define NSE_APP NSE_ZPP }
#define USE_APP using namespace z::app;
#endif

#ifndef NSB_CORO
#define NSB_CORO NSB_ZPP namespace coro{
#define NSE_CORO NSE_ZPP }
#define USE_CORO using namespace z::coro;
#endif

#ifndef ENABLE_CUDA
#define ENABLE_CUDA 0
#endif