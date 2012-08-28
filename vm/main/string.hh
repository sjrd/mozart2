// Copyright © 2012, Université catholique de Louvain
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// *  Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// *  Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __STRING_H
#define __STRING_H

#include <string>
#include "mozartcore.hh"

#ifndef MOZART_GENERATOR

namespace mozart {

////////////
// String //
////////////

#include "String-implem.hh"

// Core methods ----------------------------------------------------------------

String::String(VM vm, GR gr, Self from)
  : _string(vm, from->_string) {}

bool String::equals(VM vm, Self right) {
  return _string == right->_string;
}

// Comparable ------------------------------------------------------------------

void String::compare(Self self, VM vm, RichNode right, int& result) {
  LString<nchar>* rightString = nullptr;
  StringLike(right).stringGet(vm, rightString);
  result = compareByCodePoint(_string, *rightString);
}

// StringLike ------------------------------------------------------------------

void String::stringGet(Self self, VM vm, LString<nchar>*& result) {
  result = &_string;
}

void String::stringGet(Self self, VM vm, LString<unsigned char>*& result) {
  return raiseTypeError(vm, MOZART_STR("ByteString"), self);
}

void String::stringCharAt(Self self, VM vm, RichNode indexNode,
                          nativeint& character) {
  nativeint index = 0;
  getArgument(vm, index, indexNode, MOZART_STR("integer"));

  LString<nchar> slice = sliceByCodePointsFromTo(_string, index, index+1);
  if (slice.isError()) {
    if (slice.error == UnicodeErrorReason::indexOutOfBounds)
      return raiseIndexOutOfBounds(vm, indexNode, self);
    else
      return raiseUnicodeError(vm, slice.error, self);
  }

  char32_t codePoint;
  nativeint length;
  std::tie(codePoint, length) = fromUTF(slice.string, slice.length);
  if (length <= 0)
    return raiseUnicodeError(vm, (UnicodeErrorReason) length, self, indexNode);

  character = codePoint;
}

void String::stringAppend(Self self, VM vm, RichNode right,
                          UnstableNode& result) {
  LString<nchar>* rightString = nullptr;
  StringLike(right).stringGet(vm, rightString);
  LString<nchar> resultString = concatLString(vm, _string, *rightString);
  if (resultString.isError())
    return raiseUnicodeError(vm, resultString.error, self, right);
  result = String::build(vm, resultString);
}

void String::stringSlice(Self self, VM vm, RichNode from, RichNode to,
                         UnstableNode& result) {
  nativeint fromIndex = 0, toIndex = 0;
  getArgument(vm, fromIndex, from, MOZART_STR("integer"));
  getArgument(vm, toIndex, to, MOZART_STR("integer"));

  LString<nchar> resultString =
    sliceByCodePointsFromTo(_string, fromIndex, toIndex);

  if (resultString.isError()) {
    if (resultString.error == UnicodeErrorReason::indexOutOfBounds)
      return raiseIndexOutOfBounds(vm, self, from, to);
    else
      return raiseUnicodeError(vm, resultString.error, self);
  }

  result = String::build(vm, resultString);
}

void String::stringSearch(Self self, VM vm, RichNode from, RichNode needleNode,
                          UnstableNode& begin, UnstableNode& end) {
  nativeint fromIndex = 0;
  getArgument(vm, fromIndex, from, MOZART_STR("integer"));

  nchar utf[4];
  mut::BaseLString<nchar> needleStorage;
  BaseLString<nchar>* needle;

  // Extract the needle. Could be a code point, or a string.
  {
    using namespace patternmatching;
    nativeint codePointInteger = 0;
    if (matches(vm, needleNode, capture(codePointInteger))) {

      char32_t codePoint = (char32_t) codePointInteger;
      nativeint length = toUTF(codePoint, utf);
      if (length <= 0)
        return raiseUnicodeError(vm, (UnicodeErrorReason) length, needleNode);
      needle = new (&needleStorage) BaseLString<nchar> (utf, length);

#ifdef _LIBCPP_TYPE_TRAITS
      static_assert(std::is_trivially_destructible<BaseLString<nchar>>::value,
                    "BaseLString<nchar> has been modified to have non-trivial "
                    "destructor! Please rewrite this piece of code to avoid "
                    "resource leak.");
      // ^ BaseLString<nchar> has trivial destructor, so we shouldn't need to
      //   explicitly destroy it.
      //   Note: libstdc++ before 4.8 still calls it 'std::has_trivial_destructor'.
#endif

    } else {

      LString<nchar>* stringNeedle = nullptr;
      StringLike(needleNode).stringGet(vm, stringNeedle);
      needle = stringNeedle;

    }
  }

  // Do the actual searching.
  LString<nchar> haystack = sliceByCodePointsFrom(_string, fromIndex);

  if (haystack.isError()) {
    if (haystack.error == UnicodeErrorReason::indexOutOfBounds)
      return raiseIndexOutOfBounds(vm, self, from);
    else
      return raiseUnicodeError(vm, haystack.error, self);
  }

  const nchar* foundIter = std::search(haystack.begin(), haystack.end(),
                                       needle->begin(), needle->end());

  // Make result
  if (foundIter == haystack.end()) {
    begin = Boolean::build(vm, false);
    end = Boolean::build(vm, false);
  } else {
    LString<nchar> haystackUntilNeedle =
      haystack.slice(0, foundIter-haystack.begin());
    nativeint foundIndex = fromIndex + codePointCount(haystackUntilNeedle);

    begin = SmallInt::build(vm, foundIndex);
    end = SmallInt::build(vm, foundIndex + codePointCount(*needle));
  }
}

void String::stringHasPrefix(Self self, VM vm, RichNode prefixNode,
                             bool& result) {
  LString<nchar>* prefix = nullptr;
  StringLike(prefixNode).stringGet(vm, prefix);
  if (_string.length < prefix->length)
    result = false;
  else
    result = (memcmp(_string.string, prefix->string,
                     prefix->bytesCount()) == 0);
}

void String::stringHasSuffix(Self self, VM vm, RichNode suffixNode,
                             bool& result) {
  LString<nchar>* suffix = nullptr;
  StringLike(suffixNode).stringGet(vm, suffix);
  if (_string.length < suffix->length)
    result = false;
  else
    result = (memcmp(_string.end() - suffix->length, suffix->string,
                     suffix->bytesCount()) == 0);
}

// Dottable --------------------------------------------------------------------

void String::lookupFeature(Self self, VM vm, RichNode feature,
                           bool& found, nullable<UnstableNode&> value) {
  using namespace patternmatching;

  nativeint featureIntValue = 0;

  // Fast-path for the integer case
  if (matches(vm, feature, capture(featureIntValue))) {
    return lookupFeature(self, vm, featureIntValue, found, value);
  } else {
    requireFeature(vm, feature);
    found = false;
  }
}

void String::lookupFeature(Self self, VM vm, nativeint feature,
                           bool& found, nullable<UnstableNode&> value) {
  LString<nchar> slice = sliceByCodePointsFromTo(_string, feature, feature+1);
  if (slice.isError()) {
    if (slice.error == UnicodeErrorReason::indexOutOfBounds) {
      found = false;
      return;
    } else {
      return raiseUnicodeError(vm, slice.error, self);
    }
  }

  char32_t codePoint;
  nativeint length;
  std::tie(codePoint, length) = fromUTF(slice.string, slice.length);
  if (length <= 0)
    return raiseUnicodeError(vm, (UnicodeErrorReason) length, self, feature);

  found = true;
  if (value.isDefined())
    value.get() = mozart::build(vm, (nativeint) codePoint);
}

// VirtualString ---------------------------------------------------------------

void String::toString(Self self, VM vm, std::basic_ostream<nchar>& sink) {
  sink << _string;
}

void String::vsLength(Self self, VM vm, nativeint& result) {
  result = codePointCount(_string);
}

// Miscellaneous ---------------------------------------------------------------

void String::printReprToStream(Self self, VM vm, std::ostream& out,
                               int depth) {
  out << '"' << toUTF<char>(_string) << '"';
}

}

#endif // MOZART_GENERATOR

#endif // __STRING_H
