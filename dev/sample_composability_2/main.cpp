#include <so_5/all.hpp>
#include <so_5_extra/async_op/time_limited.hpp>

#include <optional>
#include <random>
#include <unordered_map>

using request_id_t = std::uint32_t;

using namespace std::literals;

struct ping {
	so_5::mbox_t reply_to;
};

struct pong {};

struct timeout {};

class pinger final : public so_5::agent_t {
	so_5::mbox_t ponger_;

	std::size_t pings_left;
	std::size_t pings_success = 0;
	std::size_t pings_error = 0;

	void handle_results() {
		if (pings_left == 0) {
			std::cout << "success: " << pings_success << ", errors: " << pings_error
						 << "\n";
			so_deregister_agent_coop_normally();
		}
	}

	void record_success() {
		++pings_success;
		--pings_left;
		handle_results();
	}

	void record_fail() {
		++pings_error;
		--pings_left;
		handle_results();
	}

	void next_ping() {
		const auto reply_mbox = so_make_new_direct_mbox();
		auto cp = so_5::extra::async_op::time_limited::make<timeout>(*this)
			.completed_on(reply_mbox, so_default_state(),
					[this](mhood_t<pong>) { record_success(); })
			.default_timeout_handler(
					[this](mhood_t<timeout>) { record_fail(); })
			.activate(200ms);

		try {
			so_5::send<ping>(ponger_, reply_mbox);
		}
		catch(...) {
			cp.cancel();
			throw;
		}
	}

  public:
	pinger(context_t ctx, std::size_t pings)
		: so_5::agent_t{std::move(ctx)}
		, pings_left{pings}
	{}

	void set_ponger(const so_5::mbox_t mbox) { ponger_ = mbox; }

	void so_evt_start() override {
		for(std::size_t i = 0u; i != pings_left; ++i)
			next_ping();
	}
};

class ponger final : public so_5::agent_t {
	std::random_device rd;
	std::mt19937 gen;
	std::uniform_real_distribution<> distr;

  public:
	ponger(context_t ctx)
		: so_5::agent_t{std::move(ctx)}
		, gen(rd())
	{}

	void so_define_agent() override {
		so_subscribe_self().event([this](mhood_t<ping> cmd) {
			auto dice_roll = distr(gen);
			std::cout << "ponger::on_ping " << dice_roll << "\n";
			if (dice_roll > 0.5) {
				std::cout << "ponger::on_ping (sending pong back)" << std::endl;
				so_5::send<pong>(cmd->reply_to);
			}
		});
	}
};

int main() {
	so_5::launch([](so_5::environment_t &env) {
		env.introduce_coop([](so_5::coop_t &coop) {
			auto pinger_actor = coop.make_agent<pinger>(5);
			auto ponger_actor = coop.make_agent<ponger>();

			pinger_actor->set_ponger(ponger_actor->so_direct_mbox());
		});
	});

	return 0;
}

