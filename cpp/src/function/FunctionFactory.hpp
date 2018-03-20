#ifndef _FunctionFactory_
#define _FunctionFactory_

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include "../types.hpp"
#include "../IllegalArgumentException.hpp"
#include "../transform/BWT.hpp"
#include "../transform/BWTS.hpp"
#include "../transform/MTFT.hpp"
#include "../transform/SBRT.hpp"
#include "TransformSequence.hpp"
#include "BWTBlockCodec.hpp"
#include "SnappyCodec.hpp"
#include "LZ4Codec.hpp"
#include "ZRLT.hpp"
#include "RLT.hpp"
#include "NullFunction.hpp"
#include "TextCodec.hpp"
#include "X86Codec.hpp"

namespace kanzi {

template <class T>
   class FunctionFactory {
   public:
       // Up to 15 transforms can be declared (4 bit index)
       static const uint32 NONE_TYPE = 0; // copy
       static const uint32 BWT_TYPE = 1; // Burrows Wheeler
       static const uint32 BWTS_TYPE = 2; // Burrows Wheeler Scott
       static const uint32 LZ4_TYPE = 3; // LZ4
       static const uint32 SNAPPY_TYPE = 4; // Snappy
       static const uint32 RLT_TYPE = 5; // Run Length
       static const uint32 ZRLT_TYPE = 6; // Zero Run Length
       static const uint32 MTFT_TYPE = 7; // Move To Front
       static const uint32 RANK_TYPE = 8; // Rank
       static const uint32 X86_TYPE = 9; // X86 codec
       static const uint32 DICT_TYPE = 10; // Text codec

       FunctionFactory() {}

       ~FunctionFactory() {}

       uint32 getType(const char* name) const THROW;

       uint32 getTypeToken(const char* name) const THROW;

       string getName(uint32 functionType) const THROW;

       static TransformSequence<T>* newFunction(map<string, string>& ctx, uint32 functionType) THROW;

   private:
       static Transform<T>* newFunctionToken(map<string, string>& ctx, uint32 functionType) THROW;

       static const char* getNameToken(uint32 functionType) THROW;
   };

   // The returned type contains 8 (nibble based) transform values
   template <class T>
   uint32 FunctionFactory<T>::getType(const char* cname) const THROW
   {
       string name(cname);

       if (name.find("+") == string::npos)
           return getTypeToken(name.c_str()) << 28;

       char buf[64];
       int length = (name.length() < 63) ? int(name.length()) : 63;
       memcpy(buf, name.c_str(), length);
       buf[length] = 0;
       const char* token = strtok(buf, "+");

       if (token == nullptr) {
           stringstream ss;
           ss << "Unknown transform type: " << name;
           throw IllegalArgumentException(ss.str());
       }

       uint32 res = 0;
       int shift = 28;
       int n = 0;

       while (token != nullptr) {
           short typeTk = getTypeToken(token);
           n++;

           if (n > 8) {
               stringstream ss;
               ss << "Only 8 transforms allowed: " << name;
               throw IllegalArgumentException(ss.str());
           }

           // Skip null transform
           if (typeTk != NONE_TYPE) {
               res |= (typeTk << shift);
               shift -= 4;
           }

           token = strtok(nullptr, "+");
       }

       return res;
   }

   template <class T>
   uint32 FunctionFactory<T>::getTypeToken(const char* cname) const THROW
   {
       string name(cname);
       transform(name.begin(), name.end(), name.begin(), ::toupper);

       if (name.compare("BWT") == 0)
           return BWT_TYPE;

       if (name.compare("BWTS") == 0)
           return BWTS_TYPE;

       if (name.compare("SNAPPY") == 0)
           return SNAPPY_TYPE;

       if (name.compare("LZ4") == 0)
           return LZ4_TYPE;

       if (name.compare("MTFT") == 0)
           return MTFT_TYPE;

       if (name.compare("ZRLT") == 0)
           return ZRLT_TYPE;

       if (name.compare("RLT") == 0)
           return RLT_TYPE;

       if (name.compare("RANK") == 0)
           return RANK_TYPE;

       if (name.compare("TEXT") == 0)
           return DICT_TYPE;

       if (name.compare("X86") == 0)
           return X86_TYPE;

       if (name.compare("NONE") == 0)
           return NONE_TYPE;

       stringstream ss;
       ss << "Unknown transform type: " << name;
       throw IllegalArgumentException(ss.str());
   }

   template <class T>
   TransformSequence<T>* FunctionFactory<T>::newFunction(map<string, string>& ctx, uint32 functionType) THROW
   {
       int nbtr = 0;

       // Several transforms
       for (int i = 0; i < 8; i++) {
           if (((functionType >> (28 - 4 * i)) & 0x0F) != NONE_TYPE)
               nbtr++;
       }

       // Only null transforms ? Keep first.
       if (nbtr == 0)
           nbtr = 1;

       Transform<T>* transforms[8];
       nbtr = 0;

       for (int i = 0; i < 8; i++) {
           transforms[i] = nullptr;
           uint32 t = (functionType >> (28 - 4 * i)) & 0x0F;

           if ((t != NONE_TYPE) || (i == 0))
               transforms[nbtr++] = newFunctionToken(ctx, t);
       }

       return new TransformSequence<T>(transforms, true);
   }

   template <class T>
   Transform<T>* FunctionFactory<T>::newFunctionToken(map<string, string>& ctx, uint32 functionType) THROW
   {
       switch (functionType & 0x0F) {
       case SNAPPY_TYPE:
           return new SnappyCodec();

       case LZ4_TYPE:
           return new LZ4Codec();

       case BWT_TYPE:
           return new BWTBlockCodec();

       case BWTS_TYPE:
           return new BWTS();

       case MTFT_TYPE:
           return new MTFT();

       case ZRLT_TYPE:
           return new ZRLT();

       case RLT_TYPE:
           return new RLT();

       case RANK_TYPE:
           return new SBRT(SBRT::MODE_RANK);

       case DICT_TYPE:
           return new TextCodec(ctx);

       case X86_TYPE:
           return new X86Codec();

       case NONE_TYPE:
           return new NullFunction<T>();

       default:
           stringstream ss;
           ss << "Unknown transform type: " << functionType;
           throw IllegalArgumentException(ss.str());
       }
   }

   template <class T>
   string FunctionFactory<T>::getName(uint32 functionType) const THROW
   {
       stringstream ss;

       for (int i = 0; i < 8; i++) {
           uint32 t = functionType >> (28 - 4 * i);

           if ((t & 0x0F) == NONE_TYPE)
               continue;

           string name = getNameToken(t);

           if (ss.str().length() != 0)
               ss << "+";

           ss << name;
       }

       if (ss.str().length() == 0) {
           ss << getNameToken(NONE_TYPE);
       }

       return ss.str();
   }

   template <class T>
   const char* FunctionFactory<T>::getNameToken(uint32 functionType) THROW
   {
       switch (functionType & 0x0F) {
       case LZ4_TYPE:
           return "LZ4";

       case BWT_TYPE:
           return "BWT";

       case BWTS_TYPE:
           return "BWTS";

       case SNAPPY_TYPE:
           return "SNAPPY";

       case MTFT_TYPE:
           return "MTFT";

       case ZRLT_TYPE:
           return "ZRLT";

       case RLT_TYPE:
           return "RLT";

       case RANK_TYPE:
           return "RANK";

       case DICT_TYPE:
           return "TEXT";

       case X86_TYPE:
           return "X86";

       case NONE_TYPE:
           return "NONE";

       default:
           stringstream ss;
           ss << "Unknown transform type: " << functionType;
           throw IllegalArgumentException(ss.str());
       }
   }
}

#endif