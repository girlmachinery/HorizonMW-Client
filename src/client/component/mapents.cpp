#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"
#include "console.hpp"
#include "filesystem.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>

#include "gsc/script_loading.hpp"

namespace mapents
{
	namespace
	{
		std::optional<std::string> parse_mapents(const std::string& source)
		{
			std::string out_buffer{};

			const auto lines = utils::string::split(source, '\n');
			auto in_map_ent = false;
			auto empty = false;
			auto in_comment = false;

			for (auto i = 0; i < lines.size(); i++)
			{
				//if (*reinterpret_cast<bool*>(0x142EC8065) == false)
				//	*reinterpret_cast<bool*>(0x142EC8065) = true;

				[[maybe_unused]] auto line_num = i+1;
				auto line = lines[i];
				if (line.ends_with('\r'))
				{
					line.pop_back();
				}

				if (line.starts_with("/*"))
				{
					in_comment = true;
					continue;
				}

				if (line.ends_with("*/"))
				{
					in_comment = false;
					continue;
				}

				if (in_comment)
				{
					continue;
				}

				if (line.starts_with("//"))
				{
					continue;
				}

				if (line[0] == '{' && !in_map_ent)
				{
					in_map_ent = true;
					out_buffer.append("{\n");
					continue;
				}

				if (line[0] == '{' && in_map_ent)
				{
#ifdef DEBUG
					console::error("[map_ents parser] Unexpected '{' on line %i\n", line_num);
#endif
					return {};
				}

				if (line[0] == '}' && in_map_ent)
				{
					if (empty)
					{
						out_buffer.append("\n}\n");
					}
					else if (i < static_cast<int>(lines.size()) - 1)
					{
						out_buffer.append("}\n");
					}
					else
					{
						out_buffer.append("}\0");
					}

					in_map_ent = false;
					continue;
				}

				if (line[0] == '}' && !in_map_ent)
				{
#ifdef DEBUG
					console::error("[map_ents parser] Unexpected '}' on line %i\n", line_num);
#endif
					return {};
				}

				if (line.empty())
				{
					continue;
				}

				std::regex expr(R"~((.+) "(.*)")~");
				std::smatch match{};
				if (!std::regex_search(line, match, expr) && !line.empty())
				{
#ifdef DEBUG
					console::warn("[map_ents parser] Failed to parse line %i (%s)\n", line_num, line.data());
#endif
					continue;
				}

				auto key = utils::string::to_lower(match[1].str());
				const auto value = match[2].str();

				if (key.size() <= 0)
				{
#ifdef DEBUG
					console::warn("[map_ents parser] Invalid key ('%s') on line %i (%s)\n", key.data(), line_num, line.data());
#endif
					continue;
				}

				if (value.size() <= 0)
				{
					continue;
				}

				empty = false;

				if (utils::string::is_numeric(key) || key.size() < 3 || !key.starts_with("\"") || !key.ends_with("\""))
				{
					out_buffer.append(line);
					out_buffer.append("\n");
					continue;
				}

				const auto key_ = key.substr(1, key.size() - 2);
				const auto id = gsc::gsc_ctx->token_id(key_);
				if (id == 0)
				{
#ifdef DEBUG
					console::warn("[map_ents parser] Key '%s' not found, on line %i (%s)\n", key_.data(), line_num, line.data());
#endif
					continue;
				}

				out_buffer.append(utils::string::va("%i \"%s\"\n", id, value.data()));
			}

			//*reinterpret_cast<bool*>(0x142EC8065) = true;

			return {out_buffer};
		}

		std::string raw_ents;
		bool load_raw_mapents()
		{
			auto mapents_name = utils::string::va("%s.ents", **reinterpret_cast<const char***>(0xA975F40_b));
			if (filesystem::exists(mapents_name))
			{
				try
				{
#ifdef DEBUG
					console::debug("Reading raw ents file \"%s\"\n", mapents_name);
#endif
					raw_ents = filesystem::read_file(mapents_name);
					if (!raw_ents.empty())
					{
						return true;
					}
				}
				catch ([[maybe_unused]] const std::exception& ex)
				{
#ifdef DEBUG
					console::error("Failed to read raw ents file \"%s\"\n%s\n", mapents_name, ex.what());
#endif
				}
			}
			return false;
		}

		std::string entity_string;
		const char* cm_entity_string_stub()
		{
			const char* ents = nullptr;
			if (load_raw_mapents())
			{
				ents = raw_ents.data();
			}
			else
			{
				if (!entity_string.empty())
				{
					return entity_string.data();
				}

				ents = utils::hook::invoke<const char*>(0x4CD140_b);
			}

			const auto parsed = parse_mapents(ents);
			if (parsed.has_value())
			{
				entity_string = parsed.value();
				return entity_string.data();
			}
			else
			{
				return ents;
			}
		}

		void cm_unload_stub(void* clip_map)
		{
			entity_string.clear();
			raw_ents.clear();
			utils::hook::invoke<void>(0x4CD0E0_b, clip_map);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			utils::hook::call(0x41F594_b, cm_entity_string_stub);
			utils::hook::call(0x399814_b, cm_unload_stub);
		}
	};
}

REGISTER_COMPONENT(mapents::component)
