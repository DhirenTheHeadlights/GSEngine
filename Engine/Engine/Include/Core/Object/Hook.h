#pragma once

namespace gse {
	struct base_hook {
		virtual ~base_hook() = default;

		virtual void initialize() = 0;
		virtual void update() = 0;
		virtual void render() = 0;
	};

	template <typename owner_type>
	struct hook : base_hook {
		hook() = default;
		hook(owner_type* owner) : owner(owner) {}
		~hook() override = default;

		void set_owner(owner_type* owner) {
			this->owner = owner;
		}
	protected:
		owner_type* owner;
	};
}