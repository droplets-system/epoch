#include "epoch.drops/epoch.drops.hpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

namespace dropssystem {

[[eosio::action]] void epoch::commit(const name oracle, const uint64_t epoch, const checksum256 commit)
{
   require_auth(oracle);
   check_is_enabled();

   const uint64_t current_epoch_height = get_current_epoch_height();
   check(epoch == current_epoch_height, "Epoch submitted (" + to_string(epoch) + ") is not the current epoch (" +
                                           to_string(current_epoch_height) + ").");

   ensure_epoch_advance(current_epoch_height);

   const epoch::epoch_row _epoch = get_epoch(epoch);
   check(find(_epoch.oracles.begin(), _epoch.oracles.end(), oracle) != _epoch.oracles.end(),
         "Oracle is not in the list of oracles for Epoch " + to_string(epoch) + ".");
   check(!oracle_has_committed(oracle, epoch), "Oracle has already committed");

   emplace_commit(epoch, oracle, commit);
}

[[eosio::action]] void epoch::reveal(const name oracle, const uint64_t epoch, const string reveal)
{
   require_auth(oracle);
   check_is_enabled();

   const epoch::epoch_row _epoch = get_epoch(epoch);
   check(find(_epoch.oracles.begin(), _epoch.oracles.end(), oracle) != _epoch.oracles.end(),
         "Oracle is not in the list of oracles for Epoch " + to_string(epoch) + ".");

   const uint64_t current_epoch_height = get_current_epoch_height();
   check(epoch < current_epoch_height, "Epoch (" + to_string(epoch) + ") has not completed.");

   ensure_epoch_advance(current_epoch_height);

   check(!oracle_has_revealed(oracle, epoch), "Oracle has already revealed");

   const commit_row  _commit     = get_commit(oracle, epoch);
   const checksum256 commit_hash = _commit.commit;
   const auto        commit_arr  = commit_hash.extract_as_byte_array();

   const checksum256 reveal_hash = sha256(reveal.c_str(), reveal.length());
   const auto        reveal_arr  = reveal_hash.extract_as_byte_array();

   check(reveal_hash == commit_hash,
         "Reveal value '" + reveal + "' hashes to '" + hexStr(reveal_arr.data(), reveal_arr.size()) +
            "' which does not match commit value '" + hexStr(commit_arr.data(), commit_arr.size()) + "'.");

   emplace_reveal(epoch, oracle, reveal);

   ensure_epoch_reveal(epoch);
}

void epoch::check_is_enabled()
{
   epoch::state_table _state(get_self(), get_self().value);
   const auto         state = _state.get_or_default();
   check(state.enabled, ERROR_SYSTEM_DISABLED);
}

epoch::epoch_row epoch::get_epoch(const uint64_t epoch)
{
   epoch::epoch_table _epoch(get_self(), get_self().value);
   const auto         epoch_itr = _epoch.find(epoch);
   check(epoch_itr != _epoch.end(), "Epoch " + to_string(epoch) + " does not exist.");
   return *epoch_itr;
}

uint64_t epoch::get_current_epoch_height()
{
   epoch::state_table _state(get_self(), get_self().value);
   const auto         state = _state.get_or_default();
   return derive_epoch(state.genesis, state.duration);
}

bool epoch::oracle_has_committed(const name oracle, const uint64_t epoch)
{
   epoch::commit_table commits(get_self(), get_self().value);
   const auto          commit_idx = commits.get_index<"epochoracle"_n>();
   const auto          commit_itr = commit_idx.find(((uint128_t)oracle.value << 64) + epoch);
   return commit_itr != commit_idx.end();
}

bool epoch::oracle_has_revealed(const name oracle, const uint64_t epoch)
{
   epoch::reveal_table reveals(get_self(), get_self().value);
   const auto          reveal_idx = reveals.get_index<"epochoracle"_n>();
   const auto          reveal_itr = reveal_idx.find(((uint128_t)oracle.value << 64) + epoch);
   return reveal_itr != reveal_idx.end();
}

void epoch::emplace_commit(const uint64_t epoch, const name oracle, const checksum256 commit)
{
   epoch::commit_table commits(get_self(), get_self().value);
   commits.emplace(oracle, [&](auto& row) {
      row.id     = commits.available_primary_key();
      row.epoch  = epoch;
      row.oracle = oracle;
      row.commit = commit;
   });
}

void epoch::emplace_reveal(const uint64_t epoch, const name oracle, const string reveal)
{
   epoch::reveal_table reveals(get_self(), get_self().value);
   reveals.emplace(oracle, [&](auto& row) {
      row.id     = reveals.available_primary_key();
      row.epoch  = epoch;
      row.oracle = oracle;
      row.reveal = reveal;
   });
}

void epoch::ensure_epoch_advance(const uint64_t current_epoch)
{
   // Attempt to find current epoch from state in oracle contract
   epoch::epoch_table epochs(get_self(), get_self().value);
   const auto         epochs_itr = epochs.find(current_epoch);

   // If the epoch does not exist in the oracle contract, advance the epoch
   if (epochs_itr == epochs.end()) {
      epoch::advance_epoch();
   }
}

vector<name> epoch::get_active_oracles()
{
   vector<name>        oracles;
   epoch::oracle_table oracle_table(get_self(), get_self().value);
   auto                oracle_itr = oracle_table.begin();
   check(oracle_itr != oracle_table.end(), "No active oracles");
   while (oracle_itr != oracle_table.end()) {
      oracles.push_back(oracle_itr->oracle);
      oracle_itr++;
   }
   return oracles;
}

epoch::epoch_row epoch::advance_epoch()
{
   const uint64_t current_epoch_height = get_current_epoch_height();

   epoch::epoch_table epochs(get_self(), get_self().value);
   const auto         epochs_itr = epochs.find(current_epoch_height);
   check(epochs_itr == epochs.end(), "Epoch " + to_string(current_epoch_height) + " is already initialized.");

   const vector<name> oracles = get_active_oracles();

   epochs.emplace(get_self(), [&](auto& row) {
      row.epoch   = current_epoch_height;
      row.oracles = oracles;
   });

   // Return the next epoch
   return {
      current_epoch_height, // epoch
      oracles,              // oracles
   };
}

[[eosio::action]] epoch::epoch_row epoch::advance()
{
   // Only the drops contract can advance the oracle contract
   require_auth(get_self());

   // Advance the epoch
   const auto new_epoch = advance_epoch();

   // Provide the epoch as a return value
   return new_epoch;
}

epoch::reveal_row epoch::get_reveal(const name oracle, const uint64_t epoch)
{
   epoch::reveal_table reveals(get_self(), get_self().value);
   const auto          reveal_idx = reveals.get_index<"epochoracle"_n>();
   const auto          reveal_itr = reveal_idx.find(((uint128_t)oracle.value << 64) + epoch);
   check(reveal_itr != reveal_idx.end(), "Oracle has not revealed");
   return *reveal_itr;
}

epoch::commit_row epoch::get_commit(const name oracle, const uint64_t epoch)
{
   epoch::commit_table commits(get_self(), get_self().value);
   const auto          commit_idx = commits.get_index<"epochoracle"_n>();
   const auto          commit_itr = commit_idx.find(((uint128_t)oracle.value << 64) + epoch);
   check(commit_itr != commit_idx.end(), "Oracle has not committed");
   return *commit_itr;
}

void epoch::remove_oracle_commit(const uint64_t epoch, const name oracle)
{
   epoch::commit_table _commits(get_self(), get_self().value);
   auto                commit_idx = _commits.get_index<"epochoracle"_n>();
   auto                commit_itr = commit_idx.find(((uint128_t)oracle.value << 64) + epoch);
   if (commit_itr != commit_idx.end()) {
      commit_idx.erase(commit_itr);
   }
}

void epoch::remove_oracle_reveal(const uint64_t epoch, const name oracle)
{
   epoch::reveal_table _reveals(get_self(), get_self().value);
   auto                reveal_idx = _reveals.get_index<"epochoracle"_n>();
   auto                reveal_itr = reveal_idx.find(((uint128_t)oracle.value << 64) + epoch);
   if (reveal_itr != reveal_idx.end()) {
      reveal_idx.erase(reveal_itr);
   }
}

void epoch::cleanup_epoch(const uint64_t epoch, const vector<name> oracles)
{
   for (const name oracle : oracles) {
      remove_oracle_commit(epoch, oracle);
      remove_oracle_reveal(epoch, oracle);
   }
}

vector<string> epoch::get_epoch_reveals(const uint64_t epoch)
{
   const epoch_row selected_epoch = get_epoch(epoch);
   vector<string>  reveals;

   const reveal_table _reveals(get_self(), get_self().value);
   auto               idx       = _reveals.get_index<"epoch"_n>();
   auto               itr_start = idx.lower_bound(epoch);
   auto               itr_end   = idx.upper_bound(epoch);

   for (auto itr = idx.begin(); itr != idx.end(); itr++) {
      if (itr->epoch != epoch)
         continue;
      reveals.push_back(itr->reveal);
   }

   return reveals;
}

vector<checksum256> epoch::get_epoch_commits(const uint64_t epoch)
{
   const epoch_row     selected_epoch = get_epoch(epoch);
   vector<checksum256> commits;

   const commit_table _commits(get_self(), get_self().value);
   auto               idx       = _commits.get_index<"epoch"_n>();
   auto               itr_start = idx.lower_bound(epoch);
   auto               itr_end   = idx.upper_bound(epoch);

   for (auto itr = idx.begin(); itr != idx.end(); itr++) {
      if (itr->epoch != epoch)
         continue;
      commits.push_back(itr->commit);
   }

   return commits;
}

[[eosio::action, eosio::read_only]] checksum256 epoch::computehash(const uint64_t epoch, const vector<string> reveals)
{
   // Sort the reveal values alphebetically for consistency
   vector<string> sorted_reveals = reveals;
   sort(sorted_reveals.begin(), sorted_reveals.end());

   // Combine the epoch, drops, and reveals into a single string
   string result = to_string(epoch);
   for (const auto& reveal : sorted_reveals)
      result += reveal;

   return sha256(result.c_str(), result.length());
}

void epoch::complete_epoch(const uint64_t epoch, const checksum256 epoch_seed)
{
   epoch::epoch_table _epoch(get_self(), get_self().value);
   auto&              epoch_row = _epoch.get(epoch, "Epoch not found");
   _epoch.modify(epoch_row, get_self(), [&](auto& row) {
      row.oracles = {};
      row.seed    = epoch_seed;
   });
}

void epoch::ensure_epoch_reveal(const uint64_t epoch)
{
   const epoch_row           selected_epoch = get_epoch(epoch);
   const vector<checksum256> commits        = get_epoch_commits(epoch);
   const vector<string>      reveals        = get_epoch_reveals(epoch);

   if (reveals.size() == commits.size()) {
      const auto seed = computehash(epoch, reveals);
      complete_epoch(epoch, seed);
      cleanup_epoch(epoch, selected_epoch.oracles);
   }
}

[[eosio::action]] void epoch::addoracle(const name oracle)
{
   require_auth(get_self());
   check(is_account(oracle), "Account does not exist.");
   epoch::oracle_table oracles(get_self(), get_self().value);
   oracles.emplace(get_self(), [&](auto& row) { row.oracle = oracle; });
}

[[eosio::action]] void epoch::removeoracle(const name oracle)
{
   require_auth(get_self());

   epoch::oracle_table oracles(get_self(), get_self().value);
   const auto          oracle_itr = oracles.find(oracle.value);
   check(oracle_itr != oracles.end(), "Oracle not found");
   oracles.erase(oracle_itr);
}

[[eosio::action]] void epoch::init()
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();

   const block_timestamp genesis =
      time_point_sec((current_time_point().sec_since_epoch() / state.duration) * state.duration);

   state.enabled = true;
   state.genesis = genesis;
   _state.set(state, get_self());

   // Load oracles to initialize the first epoch
   epoch::oracle_table oracle_table(get_self(), get_self().value);
   vector<name>        oracles;
   auto                oracle_itr = oracle_table.begin();
   check(oracle_itr != oracle_table.end(), "No oracles registered, cannot init.");
   while (oracle_itr != oracle_table.end()) {
      oracles.push_back(oracle_itr->oracle);
      oracle_itr++;
   }

   // Add the initial epoch row to the oracle contract
   epoch::epoch_table epochs(get_self(), get_self().value);
   epochs.emplace(get_self(), [&](auto& row) {
      row.epoch   = 1;
      row.oracles = oracles;
   });
}

// @admin
[[eosio::action]] void epoch::enable(const bool enabled)
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();
   state.enabled            = enabled;
   _state.set(state, get_self());
}

// @admin
[[eosio::action]] void epoch::duration(const uint32_t duration)
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();
   state.duration           = duration;
   _state.set(state, get_self());
}

[[eosio::action, eosio::read_only]] uint64_t epoch::getepoch() { return get_current_epoch_height(); }

[[eosio::action, eosio::read_only]] vector<name> epoch::getoracles()
{
   const uint64_t         current_epoch_height = get_current_epoch_height();
   const epoch::epoch_row _epoch               = get_epoch(current_epoch_height);
   return _epoch.oracles;
}

[[eosio::action]] void epoch::wipe()
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   _state.remove();
   auto state = _state.get_or_default();
   _state.set(state, get_self());

   epoch::commit_table commits(get_self(), get_self().value);
   auto                commit_itr = commits.begin();
   while (commit_itr != commits.end()) {
      commit_itr = commits.erase(commit_itr);
   }

   epoch::epoch_table epochs(get_self(), get_self().value);
   auto               epoch_itr = epochs.begin();
   while (epoch_itr != epochs.end()) {
      epoch_itr = epochs.erase(epoch_itr);
   }

   epoch::reveal_table reveals(get_self(), get_self().value);
   auto                reveal_itr = reveals.begin();
   while (reveal_itr != reveals.end()) {
      reveal_itr = reveals.erase(reveal_itr);
   }

   epoch::oracle_table oracles(get_self(), get_self().value);
   auto                oracle_itr = oracles.begin();
   while (oracle_itr != oracles.end()) {
      oracle_itr = oracles.erase(oracle_itr);
   }
}

} // namespace dropssystem
