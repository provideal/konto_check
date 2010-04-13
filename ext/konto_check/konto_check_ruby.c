// Copyright (c) 2010 Provideal Systems GmbH
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Include the Ruby headers and goodies
#include "ruby.h"
#include "konto_check.h"

// Defining a space for information and references about the module to be stored internally
VALUE KontoCheck = Qnil;

// Our 'kontocheck' method
VALUE konto_check(VALUE self, VALUE account, VALUE bic) {
//  lut_init("blz.lut",5,0);
  char* kto = RSTRING(account)->ptr;
  char* blz = RSTRING(bic)->ptr;
  if (1==kto_check_blz(blz, kto))
    return Qtrue;
  return Qfalse;
}

VALUE bank_name(VALUE self, VALUE bic) {
  char* blz = RSTRING(bic)->ptr;
  return rb_str_new2("hahaa!");
}

// The initialization method for this module
void Init_kontocheck() {
  KontoCheck = rb_define_module("KontoCheck");
  rb_define_module_function(KontoCheck, "konto_check", konto_check, 2);
  rb_define_module_function(KontoCheck, "bank_name", bank_name, 1);
  lut_init("blz.lut",5,0);
}



