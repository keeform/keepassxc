#  Copyright (C) 2026   dave_keepass at users.sourceforge.net
#                       https://keeform.org
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

if(KPXC_FEATURE_KEEPUSH)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    set(keepushxc_SOURCES
        KeePushConnect.cpp
        KeePushSettingsPage.cpp
        KeePushSettingsWidget.cpp
    )
    if(WIN32)
        list(APPEND keepushxc_SOURCES KeePushConnect_windows.cpp)
    else()
        list(APPEND keepushxc_SOURCES KeePushConnect_nix.cpp)
    endif()
    add_library(keepushxc STATIC ${keepushxc_SOURCES})
    target_link_libraries(keepushxc Qt5::Core Qt5::Widgets Qt5::Network)
    if(WIN32)
        target_link_libraries(keepushxc crypt32)
    endif()
endif()
