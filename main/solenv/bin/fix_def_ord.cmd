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

this script is used to process def results.
Adds ordinal number to every line.

*/

lmax = 0
smax = ''

ord = 1
do while( lines())

	l = linein()
	IF LENGTH(l)>0 THEN DO
          resident = 'RESIDENTNAME'
          if POS('_GetVersionInfo',l) >0 THEN resident = ''
          if POS('_component_getFactory',l) >0 THEN resident = ''
          if POS('_component_getImplementationEnvironment',l) >0 THEN resident = ''
	  say l ' @'ord ' ' resident
	  ord = ord + 1
	END
end
