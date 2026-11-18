export module gse.audio:material;

import std;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.math;

import :mixer;

export namespace gse {
	struct acoustic_box {
		vec3<displacement> size;
		raw_blob_owned<audio::resonance> rings;
		raw_blob_owned<vec3f> axis_shares;
		raw_blob_owned<float> shapes;
	};

	class acoustic_material : public identifiable {
	public:
		struct [[
			= asset_format::baked_ext<".gacm">{},
			= asset_format::baked_dir<"Acoustics">{},
			= asset_format::source_dir<"Acoustics">{},
			= asset_format::source_exts<".acoustic">{},
			= asset_format::magic<0x4741434D>{},
			= asset_format::version<1>{}
		]] baked {
			density mass_density;
			elastic_modulus young_modulus;
			float poisson_ratio = 0.f;
			inverse_time mass_damping;
			time stiffness_damping;
			std::vector<acoustic_box> boxes;
		};

		explicit acoustic_material(
			const std::filesystem::path& filepath
		);

		auto load(
			asset::load_ctx& ctx
		) -> async::task<asset_result>;

		auto properties() const -> const baked&;

		auto box_of(
			const vec3<displacement>& size
		) const -> const acoustic_box*;

	private:
		std::filesystem::path m_path;
		baked m_baked;
	};

	auto bake(
		const std::filesystem::path& src,
		acoustic_material::baked& out
	) -> bool;
}

export namespace gse::audio {
	struct acoustic_material_component {
		id material;
		percentage<float> gain = percentage<float>::one();
	};

	auto mode_shape(
		const acoustic_box& box,
		std::size_t mode
	) -> std::span<const float>;
}
