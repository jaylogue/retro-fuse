#!/usr/bin/env python3

#
# Copyright 2021 Jay Logue
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

# 
# @file  Utility code used in retro-fuse tests.
# 

class obj:
    '''Convert a dictionary into an object with corresponding property names'''
    def __init__(self, d):
        for key, val in d.items():
            if isinstance(val, dict):
                self.__dict__.update({key: obj(val)})
            elif isinstance(val, list):
                objList = []
                for elem in val:
                    if isinstance(elem, dict):
                        objList.append(obj(elem))
                    else:
                        objList.append(elem)
                self.__dict__.update({key: objList})
            else:
                self.__dict__.update({key: val})
