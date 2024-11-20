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
		~Hook() override = default;

		void initialize() override = 0;
		void update() override = 0;
		void render() override = 0;

		void setOwner(std::shared_ptr<OwnerType> owner) {
			this->owner = owner;
		}
	protected:
		std::weak_ptr<OwnerType> owner;
	};
}