#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../core/IStream.hpp"
#include "StexObject.h"

extern "C"
{
    #include "../localisation/localisation.h"
}

enum OBJ_STRING_ID
{
    OBJ_STRING_ID_SCENARIO_NAME,
    OBJ_STRING_ID_PARK_NAME,
    OBJ_STRING_ID_SCENARIO_DETAILS,
};

void StexObject::ReadLegacy(IStream * stream)
{
    _legacyType.scenario_name = stream->ReadValue<rct_string_id>();
    _legacyType.park_name = stream->ReadValue<rct_string_id>();
    _legacyType.details = stream->ReadValue<rct_string_id>();
    _legacyType.var_06 = stream->ReadValue<uint8>();
    stream->Seek(1, STREAM_SEEK_CURRENT);

    GetStringTable().Read(stream, OBJ_STRING_ID_SCENARIO_NAME);
    GetStringTable().Read(stream, OBJ_STRING_ID_PARK_NAME);
    GetStringTable().Read(stream, OBJ_STRING_ID_SCENARIO_DETAILS);
}

void StexObject::Load()
{
    _legacyType.scenario_name = language_allocate_object_string(GetScenarioName());
    _legacyType.park_name = language_allocate_object_string(GetParkName());
    _legacyType.details = language_allocate_object_string(GetScenarioDetails());
}

void StexObject::Unload()
{
    language_free_object_string(_legacyType.scenario_name);
    language_free_object_string(_legacyType.park_name);
    language_free_object_string(_legacyType.details);
}

const utf8 * StexObject::GetName()
{
    return GetScenarioName();
}

const utf8 * StexObject::GetScenarioName()
{
    return GetStringTable().GetString(OBJ_STRING_ID_SCENARIO_NAME);
}

const utf8 * StexObject::GetScenarioDetails()
{
    return GetStringTable().GetString(OBJ_STRING_ID_SCENARIO_DETAILS);
}

const utf8 * StexObject::GetParkName()
{
    return GetStringTable().GetString(OBJ_STRING_ID_PARK_NAME);
}
