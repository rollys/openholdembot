//******************************************************************************
//
// This file is part of the OpenHoldem project
//    Source code:           https://github.com/OpenHoldem/openholdembot/
//    Forums:                http://www.maxinmontreal.com/forums/index.php
//    Licensed under GPL v3: http://www.gnu.org/licenses/gpl.html
//
//******************************************************************************
//
// Purpose: storing/recalling OH-script memory-symbols
//   and OpenPPL user-variables
//
//******************************************************************************

#define USER_VARIABLES_DLL_EXPORTS

#include "CMemorySymbolsCollection.h"
#include <assert.h>
#include "..\Debug_DLL\debug.h"
#include "..\OpenHoldem_CallBack_DLL\OpenHoldem_CallBack.h"
#include "..\Preferences_DLL\Preferences.h"
#include "..\WindowFunctions_DLL\window_functions.h"
#include "..\..\Shared\MagicNumbers\MagicNumbers.h"

CMemorySymbolsCollection::CMemorySymbolsCollection() {
}

CMemorySymbolsCollection::~CMemorySymbolsCollection() {
  _memory_symbols.clear();
}

void CMemorySymbolsCollection::UpdateOnConnection() {
  _memory_symbols.clear();
}

void CMemorySymbolsCollection::ErrorInvalidMemoryStoreCommand(CString command) {
  // Handles wrong me_st_ commands.
  CString message;
  message.Format("Invalid memory-symbol: %s\n"
    "Memory-store-commands must contain\n"
    "  * the prefix me_st_\n"
    "  * the name of the variable\n"
    "  * another underscore\n"
    "  * a value\n"
    " Example: me_st_Pi_3_141592653\n",
    command);
  ///CParseErrors::MessageBox_Formula_Error(message, "Error");
}

void CMemorySymbolsCollection::ErrorUnnamedMemorySymbol(CString command) {
  // Handles wrong me_re_ and me_inc_ commands
  // but not me_st_.
  CString message;
  message.Format("Invalid memory-symbol: %s\n"
    "Missing variable name.\n"
    "Memory-recall and memory-increment commands must contain\n"
    "  * the prefix me_re_ or me_inc_\n"
    "  * the name of the variable\n"
    " Example: me_inc_ContiBetsRaised\n",
    command);
  ///CParseErrors::MessageBox_Formula_Error(message, "Error");
}

void CMemorySymbolsCollection::Store(CString command) {
  assert(command.Left(6) == "me_st_");
  CString right_hand_side = RightHandSide(command);
  CString left_hand_side = LeftHandSide(command);
  assert(right_hand_side != "");
  assert(left_hand_side != "");
  double evaluated_right_hand_side = EvaluateRightHandExpression(right_hand_side);
  _memory_symbols[LowerCaseKey(left_hand_side)] = evaluated_right_hand_side;
}

void CMemorySymbolsCollection::Add(CString command) {
  assert(command.Left(7) == "me_add_");
  CString right_hand_side = RightHandSide(command);
  CString left_hand_side = LeftHandSide(command);
  assert(right_hand_side != "");
  assert(left_hand_side != "");
  double evaluated_right_hand_side = EvaluateRightHandExpression(right_hand_side);
  _memory_symbols[LowerCaseKey(left_hand_side)] += evaluated_right_hand_side;
}

void CMemorySymbolsCollection::Sub(CString command) {
  assert(command.Left(7) == "me_sub_");
  CString right_hand_side = RightHandSide(command);
  CString left_hand_side = LeftHandSide(command);
  assert(right_hand_side != "");
  assert(left_hand_side != "");
  double evaluated_right_hand_side = EvaluateRightHandExpression(right_hand_side);
  _memory_symbols[LowerCaseKey(left_hand_side)] -= evaluated_right_hand_side;
}

void CMemorySymbolsCollection::Increment(CString command) {
  assert(command.Left(7) == "me_inc_");
  CString left_hand_side = LeftHandSide(command);
  assert(left_hand_side != "");
  ++_memory_symbols[LowerCaseKey(left_hand_side)];
}

double CMemorySymbolsCollection::Recall(CString command) {
  assert(command.Left(6) == "me_re_");
  CString left_hand_side = LeftHandSide(command);
  assert(left_hand_side != "");
  return _memory_symbols[LowerCaseKey(left_hand_side)];
}

// Finds the Nth occurance of a char in a string
// Returns -1 if not found or found less than N times.
int FindNth(CString s, char c, int nth_occurance_to_find) {
  int next_position = 0;
  for (int i = 0; i < nth_occurance_to_find; ++i) {
    next_position = s.Find(c, next_position);
    if (next_position < 0) {
      // Not found
      return kUndefined;
    }
    // Advance to first char after finding
    ++next_position;
  }
  // Move back to last finding
  --next_position;
  return next_position;

}

CString CMemorySymbolsCollection::RightHandSide(CString command) {
  assert(command.Left(3) == "me_");
  assert(command.GetLength() > 6);
  int third_underscore = FindNth(command, '_', 3);
  if (third_underscore < 0) {
    ErrorInvalidMemoryStoreCommand(command);
    return "error";
  }
  CString result = command.Mid(third_underscore + 1);
  if (result == "") {
    ErrorInvalidMemoryStoreCommand(command);
    return "error";
  }
  return result;
}

CString CMemorySymbolsCollection::LeftHandSide(CString command) {
  assert(command.Left(3) == "me_");
  assert(command.GetLength() > 6);
  int second_underscore = FindNth(command, '_', 2);
  int third_underscore = FindNth(command, '_', 3);
  if (second_underscore < 0) {
    ErrorInvalidMemoryStoreCommand(command);
    return "error";
  }
  CString result;
  if (third_underscore < 0) {
    // No right-hand-side argument
    // Left-hand-side starts at second under-score till the very end.
    result = command.Mid(second_underscore + 1);
  }
  else {
    int left_length = third_underscore - second_underscore - 1;
    result = command.Mid(second_underscore + 1, left_length);
  }
  if (result == "") {
    ErrorInvalidMemoryStoreCommand(command);
    return "error";
  }
  return result;
}

double CMemorySymbolsCollection::EvaluateRightHandExpression(CString right_hand_value) {
  // Possible use-cases
  //   * constants        me_st_x_3_141
  //   * functions        me_st_x_f$myfunc
  //   * symbols          me_st_x_userchair
  //   * memory-symbols   me_st_x_re_re_y
  // Already removed: "me_st_x_"
  // All we have is the pure right-hand-side
  double result = kUndefinedZero;
  if (isdigit(right_hand_value[0])) {
    // Constant
    // Remove possible underscore by decimal point
    right_hand_value.Replace('_', '.');
    result = atof(right_hand_value);
  } else {
    EvaluateSymbol(right_hand_value, &result, true);
  }
  write_log(Preferences()->debug_memorysymbols(), 
    "[CMemorySymbolsCollection] Evaluating %s -> %.3f\n",
    right_hand_value, result);
  return result;
}

bool CMemorySymbolsCollection::EvaluateSymbol(const CString name, double *result, bool log /* = false */) {
  // memory-commands
  // "name" = query
  if (memcmp(name, "me_", 3) == 0) {
    write_log(Preferences()->debug_memorysymbols(), 
      "[CMemorySymbolsCollection] EvaluateSymbol(%s)\n", name);
    if (memcmp(name, "me_st_", 6) == 0) {  
      Store(name);
      *result = kUndefinedZero;
      return true;
    } else if (memcmp(name, "me_re_", 6) == 0) {
      *result = Recall(name);
      return true;
    } else if (memcmp(name, "me_inc_", 7) == 0) {
      Increment(name);
      *result = kUndefinedZero;
      return true;
    } else if (memcmp(name, "me_add_", 7) == 0) {
      Add(name);
      *result = kUndefinedZero;
      return true;
    } else if (memcmp(name, "me_sub_", 7) == 0) {
      Sub(name);
      *result = kUndefinedZero;
      return true;
    } 
    else {
    // Looks like a memory-command, but is invalid
    return false;
    }
  }
  // Not a memory symbol
  return false;
}

CString CMemorySymbolsCollection::LowerCaseKey(CString symbol) {
  CString new_key = symbol.MakeLower();
  return new_key;
}

// SymbolsProvided() does not make much sense here
// as we only know the prefixes

CMemorySymbolsCollection* memory_symbols_collection = NULL;

CMemorySymbolsCollection* MemorySymbolsCollection() {
  if (memory_symbols_collection == NULL) {
    // Lazy initialization
    memory_symbols_collection = new CMemorySymbolsCollection;
  }
  return memory_symbols_collection;
}