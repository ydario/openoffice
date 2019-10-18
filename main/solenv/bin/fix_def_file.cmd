/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/
/* os2 build scripts

this script is used to process def results

*/

lmax = 0
smax = ''
ordinal = 1

do while( lines())

	l = strip(linein())
	l = strip(l,,X2C(9))
	l = strip(l,,";")
	if LEFT( l,4) \= 'Java' THEN l = '_'l

	/* remove comments */
	if POS(';', l) > 0 then l = LEFT( l, POS(';', l)-1)
	if POS('#', l) > 0 then l = LEFT( l, POS('#', l)-1)
	/* remove wildcards */
	if POS('*', l) > 0 then l = ''

	/* remove empty lines */
	if l = '_' then l = ''

	/* remove component_getDescriptionFunc, since it is already added by tg_def */
	if l = '_component_getDescriptionFunc' then l = ''
	if l = '_GetVersionInfo' then l = ''

	/* remove GLOBAL symbols */
/*
	if POS('_GLOBAL_', l) > 0 then l = ';'l
	if POS('_ZN4_STL', l) > 0 then l = ';'l
	if POS('_ZNK4_STL', l) > 0 then l = ';'l
*/
	/* if LENGTH(l) > 254 then l = ';(>254)'left(l,100)*/

	IF LENGTH(l)>0 THEN DO
	  say l
  	  ordinal = ordinal + 1
	END

	if LENGTH(l)>lmax then do
          lmax = LENGTH(l)
          smax = l
        end

end

say ';lmax='lmax
say ';smax='smax
