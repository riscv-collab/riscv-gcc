------------------------------------------------------------------------------
--                                                                          --
--                         GNAT RUNTIME COMPONENTS                          --
--                                                                          --
--                       S Y S T E M . E X N _ G E N                        --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--                            $Revision: 1.1.16.1 $
--                                                                          --
--     Copyright (C) 1992,1993,1994,1995 Free Software Foundation, Inc.     --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT;  see file COPYING.  If not, write --
-- to  the Free Software Foundation,  59 Temple Place - Suite 330,  Boston, --
-- MA 02111-1307, USA.                                                      --
--                                                                          --
-- As a special exception,  if other files  instantiate  generics from this --
-- unit, or you link  this unit with other files  to produce an executable, --
-- this  unit  does not  by itself cause  the resulting  executable  to  be --
-- covered  by the  GNU  General  Public  License.  This exception does not --
-- however invalidate  any other reasons why  the executable file  might be --
-- covered by the  GNU Public License.                                      --
--                                                                          --
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

--  This package contains the generic functions which are instantiated with
--  predefined integer and real types to generate the runtime exponentiation
--  functions called by expanded code generated by Expand_Op_Expon. This
--  version of the package contains routines that are compiled with overflow
--  checks suppressed, so they are called for exponentiation operations which
--  do not require overflow checking

package System.Exn_Gen is
pragma Pure (System.Exn_Gen);

   --  Exponentiation for float types (checks off)

   generic
      type Type_Of_Base is digits <>;

   function Exn_Float_Type
     (Left  : Type_Of_Base;
      Right : Integer)
      return  Type_Of_Base;

   --  Exponentiation for signed integer base

   generic
      type Type_Of_Base is range <>;

   function Exn_Integer_Type
     (Left  : Type_Of_Base;
      Right : Natural)
      return  Type_Of_Base;

end System.Exn_Gen;
