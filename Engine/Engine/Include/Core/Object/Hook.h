#pragma once

namespace Engine {
	struct BaseHook {
		virtual ~BaseHook() = default;

		virtual void initialize() = 0;
		virtual void update() = 0;
		virtual void render() = 0;
	};

	template <typename OwnerType>
	struct Hook : BaseHook {
		Hook() = default;
		Hook(OwnerType* owner) : owner(owner) {}
		~Hook() override = default;

		void setOwner(OwnerType* owner) {
			this->owner = owner;
		}
	protected:
		OwnerType* owner;
	};
}