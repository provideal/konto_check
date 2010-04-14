/*==================================================================
 *
 * KontoCheck Module, C Ruby Extension
 *
 * Copyright (c) 2010 Provideal Systems GmbH
 *
 * Peter Horn, peter.horn@provideal.net
 *
 * ------------------------------------------------------------------
 *
 * ACKNOWLEDGEMENT
 *
 * This module is entirely based on the C library konto_check
 * http://www.informatik.hs-mannheim.de/konto_check/
 * http://sourceforge.net/projects/kontocheck/
 * by Michael Plugge.
 *
 * ------------------------------------------------------------------
 *
 * LICENCE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Include the Ruby headers and goodies
#include "ruby.h"
#include <stdio.h>
#include "konto_check.h"

// Defining a space for information and references about the module to be stored internally
VALUE KontoCheck = Qnil;

/**
 * KontoCheck::konto_check(<kto>, <blz>)
 *
 * check whether the given account number kto kann possibly be
 * a valid number of the bank with the bic blz.
 */
VALUE konto_check(VALUE self, VALUE kto_rb, VALUE blz_rb) {
  char* kto = RSTRING(kto_rb)->ptr;
  char* blz = RSTRING(blz_rb)->ptr;
  int ret = kto_check_blz(blz, kto);
  switch (ret) {
    case OK:
      return Qtrue;
    case LUT2_NOT_INITIALIZED:
      rb_raise(rb_eRuntimeError, "KontoCheck unitialized, use 'load_bank_data' to import bank information.");
  }
  return Qfalse;
}

/**
 * KontoCheck::load_bank_data(<datafile>)
 *
 * initialize the underlying C library konto_check with the bank
 * information from datafile.
 * Internally, this file is first transformed into a LUT file and then
 * read.
 *
 * For the datafile, use the file 'blz_yyyymmdd.txt' from
 * http://www.bundesbank.de/zahlungsverkehr/zahlungsverkehr_bankleitzahlen_download.php
 * These files are updated every three months, so be sure to
 * replace them regularly.
 */
VALUE load_bank_data(VALUE self, VALUE path_rb) {
  char* path = RSTRING(path_rb)->ptr;
  char* tmp_lut = tmpnam(NULL);

  // convert the Bankfile to a LUT file
  int ret = generate_lut(path, tmp_lut, "Temporary LUT file", 2);
  switch (ret) {
    case LUT1_FILE_GENERATED:
      break;
    case FILE_READ_ERROR:
      rb_raise(rb_eRuntimeError, "[%d] KontoCheck: can not open file '%s'.", ret, path);
    case INVALID_BLZ_FILE:
      rb_raise(rb_eRuntimeError, "[%d] KontoCheck: invalid input file '%s'. Use the file 'blz_yyyymmdd.txt' from http://www.bundesbank.de/zahlungsverkehr/zahlungsverkehr_bankleitzahlen_download.php", ret, path);
    default:
      rb_raise(rb_eRuntimeError, "[%d] KontoCheck: error reading file '%s'.", ret, tmp_lut);
  }

  // read the LUT file
  ret = kto_check_init2(tmp_lut);
  switch (ret) {
    case LUT1_SET_LOADED:
    case LUT2_PARTIAL_OK:
      return Qtrue;
    case FILE_READ_ERROR:
      rb_raise(rb_eRuntimeError, "[%d] KontoCheck: can not open tempfile '%s'.", ret, tmp_lut);
    case INVALID_LUT_FILE:
      rb_raise(rb_eRuntimeError, "[%d] KontoCheck: invalid input tempfile '%s'.", ret, tmp_lut);
  }
  rb_raise(rb_eRuntimeError, "[%d] KontoCheck: error reading tempfile '%s'.", ret, tmp_lut);
  return Qnil;
}

/**
 * KontoCheck::bank_name(<blz>)
 *
 * gives the name of the bank as a string or nil
 * if no bank with the given name exists.
 */
VALUE bank_name(VALUE self, VALUE bic) {
  char* blz = RSTRING(bic)->ptr;
  return rb_str_new2(lut_name(blz, 0, NULL));
}


/**
 * The initialization method for this module
 */
void Init_konto_check() {
  KontoCheck = rb_define_module("KontoCheck");
  rb_define_module_function(KontoCheck, "konto_check", konto_check, 2);
  rb_define_module_function(KontoCheck, "bank_name", bank_name, 1);
  rb_define_module_function(KontoCheck, "load_bank_data", load_bank_data, 1);
}

