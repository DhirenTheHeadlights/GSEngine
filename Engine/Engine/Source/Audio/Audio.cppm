export module gse.audio;

import std;

import gse.core;
import gse.concurrency;
import gse.assets;
import gse.ecs;
import gse.math;
import gse.gpu;

export namespace gse {
    class audio_clip : public identifiable {
    public:
        explicit audio_clip(
            const std::filesystem::path& filepath
        );

        auto load(
            gpu::context& context
        ) -> void;

        auto unload(
        ) -> void;

        auto data(
        ) const -> const std::vector<std::byte>&;

        auto sample_rate(
        ) const -> std::uint32_t;

        auto channels(
        ) const -> std::uint32_t;

        auto frame_count(
        ) const -> std::uint64_t;

        auto duration(
        ) const -> time_t<float, seconds>;

    private:
        std::filesystem::path m_path;
        std::vector<std::byte> m_bytes;
        std::uint32_t m_sample_rate = 0;
        std::uint32_t m_channels = 0;
        std::uint64_t m_frame_count = 0;
        time_t<float, seconds> m_duration{};
    };
}

template<>
struct gse::asset_compiler<gse::audio_clip> {
    static auto source_extensions(
    ) -> std::vector<std::string>;

    static auto baked_extension(
    ) -> std::string;

    static auto source_directory(
    ) -> std::string;

    static auto baked_directory(
    ) -> std::string;

    static auto compile_one(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool;

    static auto needs_recompile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination
    ) -> bool;

    static auto dependencies(
        const std::filesystem::path&
    ) -> std::vector<std::filesystem::path>;
};

export namespace gse {
    struct voice_handle {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;
    };
}

export namespace gse::audio {
    struct voice_slot;
    struct audio_engine;

    struct play_request {
        using result_type = voice_handle;
        const audio_clip* clip = nullptr;
        bool loop = false;
        channel_promise<voice_handle> promise;
    };

    struct stop_request {
        voice_handle handle;
    };

    struct pause_request {
        voice_handle handle;
    };

    struct resume_request {
        voice_handle handle;
    };

    struct set_volume_request {
        voice_handle handle;
        percentage<float> vol;
    };

    struct set_master_volume_request {
        percentage<float> vol;
    };

    struct state {
        std::unique_ptr<audio_engine> engine;
        bool engine_initialized = false;
        percentage<float> master_vol = percentage<float>::one();

        state();
        ~state();

        state(
            state&& other
        ) noexcept;

        auto operator=(
            state&& other
        ) noexcept -> state&;
    };

    struct system {
        struct resources {
            std::vector<std::unique_ptr<voice_slot>> voices;
            std::vector<std::uint32_t> free_list;

            resources();
            ~resources();

            resources(
                resources&& other
            ) noexcept;

            auto operator=(
                resources&& other
            ) noexcept -> resources&;
        };

        static auto initialize(
            const init_context& phase,
            resources& r,
            state& s
        ) -> void;

        static auto update(
            update_context& ctx,
            resources& r,
            state& s
        ) -> async::task<>;

        static auto shutdown(
            shutdown_context& phase,
            resources& r,
            state& s
        ) -> void;
    };

    auto allocate_voice(
        system::resources& r,
        state& s,
        const audio_clip& clip,
        bool loop
    ) -> voice_handle;

    auto release_voice(
        system::resources& r,
        voice_handle handle
    ) -> void;

    auto valid_voice(
        const system::resources& r,
        voice_handle handle
    ) -> bool;
}
