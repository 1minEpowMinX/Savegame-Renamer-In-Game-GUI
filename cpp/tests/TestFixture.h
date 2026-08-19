#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

/// Writes a .whs file with the given description XML and payload bytes.
///
/// @param path File to create, overwriting any existing one.
/// @param xml Description header, written without its terminating NUL.
/// @param payload Bytes placed after the header.
inline void MakeSave(const std::filesystem::path& path,
                     const std::string& xml,
                     const std::string& payload)
{
    const std::uint32_t magic = 0xFFFFFFFFu;
    const std::int32_t length = static_cast<std::int32_t>(xml.size() + 1);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(&magic), 4);
    f.write(reinterpret_cast<const char*>(&length), 4);
    f.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    f.put('\0');
    f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

/// Returns a description XML shaped like the header of a real permanent save.
///
/// @param saveId Value for both the SaveId attribute and UiField::Id.
/// @param quest Text for UiField::Quest, the name the load list shows.
/// @param objective Text for UiField::Objective, appended to the name after " - ".
/// @param extraAttrs Text inserted before the root element's closing bracket.
/// @return The header XML.
inline std::string SampleXml(int saveId = 3754,
                             const std::string& quest = "@qname_navsteva_lekare_sxxH",
                             const std::string& objective = "@jmena_obj_zacatek_questu_JJyn",
                             const std::string& extraAttrs = "")
{
    const std::string id = std::to_string(saveId);
    return "<C_SaveGameDescription FormatVersion=\"0\" SaveType=\"PermanentSave\" SaveId=\"" + id
         + "\" SaveTime=\"1786705444\" LevelName=\"kutnohorsko\" PlayerId=\"0\""
           " UIDescription=\"0|" + id + "|" + quest + "|" + objective
         + "|location_suchdol|1786705444|14/08/2026 13:04|60.478825|\""
           " BuildInfo=\"1.5.6-15693-release_1_5\" GameMode=\"normal\"" + extraAttrs + ">\n"
           "\t<Locations>\n"
           "\t\t<structwh::rpgmodule::S_LocationId>39a52acd</structwh::rpgmodule::S_LocationId>\n"
           "\t</Locations>\n"
           "</C_SaveGameDescription>\n";
}
