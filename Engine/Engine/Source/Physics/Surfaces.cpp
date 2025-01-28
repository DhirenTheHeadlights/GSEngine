module gse.physics.surfaces;

import std;

const std::unordered_map<gse::surfaces::surface_type, gse::surfaces::surface_properties> surface_map = {
	{ gse::surfaces::surface_type::concrete,
		{ 0.8f, 0.2f, gse::seconds(0.5f), 0.9f } },
	{ gse::surfaces::surface_type::grass,
		{ 0.6f, 0.1f, gse::seconds(0.5f), 0.7f } },
	{ gse::surfaces::surface_type::water,
		{ 0.1f, 0.0f, gse::seconds(0.5f), 0.1f } },
	{ gse::surfaces::surface_type::sand,
		{ 0.4f, 0.1f, gse::seconds(0.5f), 0.6f } },
	{ gse::surfaces::surface_type::gravel,
		{ 0.5f, 0.1f, gse::seconds(0.5f), 0.8f } },
	{ gse::surfaces::surface_type::asphalt,
		{ 0.7f, 0.2f, gse::seconds(0.5f), 0.8f } }
};

gse::surfaces::surface_properties gse::surfaces::get_surface_properties(const surface_type& surface_type) {
	return surface_map.at(surface_type);
}