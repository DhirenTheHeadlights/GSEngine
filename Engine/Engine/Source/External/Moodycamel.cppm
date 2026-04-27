module;

#include "moodycamel/concurrentqueue.h"

export module gse.moodycamel;

export namespace moodycamel {
	using ::moodycamel::ConcurrentQueue;
	using ::moodycamel::ConcurrentQueueDefaultTraits;
	using ::moodycamel::ProducerToken;
	using ::moodycamel::ConsumerToken;
}
