#include "epoch.drops/epoch.drops.hpp"

// DEBUG (used to help testing)
#ifdef DEBUG
#include "debug.cpp"
#endif

namespace dropssystem {

// checksum256 epoch::compute_epoch_value(uint64_t epoch)
// {
//    // Load the epoch and ensure all secrets have been revealed
//    epoch::epoch_table epochs(drops_contract, drops_contract.value);
//    auto               epoch_itr = epochs.find(epoch);
//    check(epoch_itr != epochs.end(), "Epoch does not exist");
//    // TODO: Check a value to ensure the epoch has been completely revealed

//    // Load all reveal values for the epoch
//    epoch::reveal_table reveal_table(_self, _self.value);
//    auto                reveal_idx = reveal_table.get_index<"epoch"_n>();
//    auto                reveal_itr = reveal_idx.find(epoch);
//    check(reveal_itr != reveal_idx.end(), "Epoch has no reveal values?");

//    // Accumulator for all reveal values
//    std::vector<std::string> reveals;

//    // Iterate over reveals and build a vector containing them all
//    while (reveal_itr != reveal_idx.end() && reveal_itr->epoch == epoch) {
//       reveals.push_back(reveal_itr->reveal);
//       reveal_itr++;
//    }

//    // Sort the reveal values alphebetically for consistency
//    sort(reveals.begin(), reveals.end());

//    // Combine the epoch, drops, and reveals into a single string
//    string result = std::to_string(epoch);
//    for (const auto& reveal : reveals)
//       result += reveal;

//    // Generate the sha256 value of the combined string
//    return sha256(result.c_str(), result.length());
// }

// checksum256 epoch::compute_epoch_drops_value(uint64_t epoch, uint64_t seed)
// {
//    // Load the drops
//    drops::drop_table drops(drops_contract, drops_contract.value);
//    auto              drops_itr = drops.find(seed);
//    check(drops_itr != drops.end(), "Drop not found");

//    // Ensure this drops was valid for the given epoch
//    // A drops must be created before or during the provided epoch
//    check(drops_itr->epoch <= epoch, "Drop was generated after this epoch and is not valid for computation.");

//    // Load the epoch drops value
//    epoch::epoch_table epoch(_self, _self.value);
//    auto               epoch_itr = epoch.find(epoch);
//    check(epoch_itr != epoch.end(), "Epoch has not yet been resolved.");

//    // Generate the sha256 value of the combined string
//    return epoch::hash(epoch_itr->drops, seed);
// }

// checksum256 epoch::compute_last_epoch_drops_value(uint64_t drops)
// {
//    // Load current state
//    //    drops::state_table state(drops_contract, drops_contract.value);
//    //    auto               state_itr = state.find(1);
//    //    uint64_t           epoch     = state_itr->epoch;
//    //    // Set to previous epoch
//    //    uint64_t last_epoch = epoch - 1;
//    //    // Return value for the last epoch
//    //    return compute_epoch_drops_value(last_epoch, drops);
// }

// [[eosio::action]] checksum256 epoch::computedrops(uint64_t epoch, uint64_t drops)
// {
//    return compute_epoch_drops_value(epoch, drops);
// }

// [[eosio::action]] checksum256 epoch::computeepoch(uint64_t epoch) { return compute_epoch_value(epoch); }

// [[eosio::action]] checksum256 epoch::cmplastepoch(uint64_t drops, name contract)
// {
//    require_recipient(contract);
//    return compute_last_epoch_drops_value(drops);
// }

void epoch::check_is_enabled()
{
   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();
   check(state.enabled, ERROR_SYSTEM_DISABLED);
}

[[eosio::action]] void epoch::commit(name oracle, uint64_t epoch, checksum256 commit)
{
   require_auth(oracle);
   check_is_enabled();

   // Retrieve contract state
   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();

   // Automatically advance if needed
   ensure_epoch_advance(state);

   // Retrieve oracle contract epoch
   epoch::epoch_table _epoch(get_self(), get_self().value);
   auto               epoch_itr = _epoch.find(epoch);
   check(epoch_itr != _epoch.end(), "Epoch does not exist in oracle contract");
   check(std::find(epoch_itr->oracles.begin(), epoch_itr->oracles.end(), oracle) != epoch_itr->oracles.end(),
         "Oracle is not in the list of oracles for this epoch");

   epoch::commit_table commits(get_self(), get_self().value);
   auto                commit_idx = commits.get_index<"epochoracle"_n>();
   auto                commit_itr = commit_idx.find(((uint128_t)oracle.value << 64) + epoch);
   check(commit_itr == commit_idx.end(), "Oracle has already committed");

   commits.emplace(get_self(), [&](auto& row) {
      row.id     = commits.available_primary_key();
      row.epoch  = epoch;
      row.oracle = oracle;
      row.commit = commit;
   });
}

void epoch::ensure_epoch_advance(epoch::state_row state)
{
   const uint64_t epoch = derive_epoch(state.genesis, state.epochlength);
   // Attempt to find current epoch from state in oracle contract
   epoch::epoch_table epochs(_self, _self.value);
   auto               epochs_itr = epochs.find(epoch);

   // If the epoch does not exist in the oracle contract, advance the epoch
   if (epochs_itr == epochs.end()) {
      epoch::epoch_row new_epoch = epoch::advance_epoch();
   }
}

// [[eosio::action]] void epoch::reveal(name oracle, uint64_t epoch, string reveal)
// {
//    require_auth(oracle);

//    // Retrieve contract state from drops contract
//    //    drops::state_table state(drops_contract, drops_contract.value);
//    //    auto               state_itr = state.find(1);
//    //    check(state_itr->enabled, "Contract is currently disabled.");

//    // Automatically advance if needed
//    ensure_epoch_advance(*state_itr);

//    // Retrieve epoch from drops contract
//    epoch::epoch_table epochs(drops_contract, drops_contract.value);
//    auto               epoch_itr = epochs.find(epoch);
//    check(epoch_itr != epochs.end(), "Epoch does not exist");
//    auto current_time = current_time_point();
//    check(current_time > epoch_itr->end, "Epoch has not concluded");

//    // Retrieve epoch from oracle contract
//    epoch::epoch_table epoch(_self, _self.value);
//    auto               epoch_itr = epoch.find(epoch);
//    check(epoch_itr != epoch.end(), "Oracle Epoch does not exist");

//    epoch::reveal_table reveals(_self, _self.value);
//    auto                reveal_idx = reveals.get_index<"epochoracle"_n>();
//    auto                reveal_itr = reveal_idx.find(((uint128_t)oracle.value << 64) + epoch);
//    check(reveal_itr == reveal_idx.end(), "Oracle has already revealed");

//    epoch::commit_table commits(_self, _self.value);
//    auto                commit_idx = commits.get_index<"epochoracle"_n>();
//    auto                commit_itr = commit_idx.find(((uint128_t)oracle.value << 64) + epoch);
//    check(commit_itr != commit_idx.end(), "Oracle never committed");

//    checksum256 reveal_hash = sha256(reveal.c_str(), reveal.length());
//    auto        reveal_arr  = reveal_hash.extract_as_byte_array();

//    checksum256 commit_hash = commit_itr->commit;
//    auto        commit_arr  = commit_hash.extract_as_byte_array();

//    check(reveal_hash == commit_hash,
//          "Reveal value '" + reveal + "' hashes to '" + hexStr(reveal_arr.data(), reveal_arr.size()) +
//             "' which does not match commit value '" + hexStr(commit_arr.data(), commit_arr.size()) + "'.");

//    reveals.emplace(_self, [&](auto& row) {
//       row.id     = reveals.available_primary_key();
//       row.epoch  = epoch;
//       row.oracle = oracle;
//       row.reveal = reveal;
//    });

//    // TODO: This logic is the exact same as finishreveal, should be refactored
//    vector<name> has_revealed;
//    auto         completed_reveals_idx = reveals.get_index<"epochoracle"_n>();
//    for (name oracle : epoch_itr->oracles) {
//       auto completed_reveals_itr = completed_reveals_idx.find(((uint128_t)oracle.value << 64) + epoch);
//       if (completed_reveals_itr != completed_reveals_idx.end()) {
//          has_revealed.push_back(oracle);
//       }
//    }
//    if (has_revealed.size() == epoch_itr->oracles.size()) {
//       // Complete the epoch
//       epoch.modify(epoch_itr, _self, [&](auto& row) {
//          row.completed = 1;
//          row.drops     = compute_epoch_value(epoch);
//       });
//    }

//    // TODO: Create an administrative action that can force an Epoch completed if
//    // an oracle fails to reveal.
// }

// [[eosio::action]] void epoch::finishreveal(uint64_t epoch)
// {
//    epoch::epoch_table epochs(drops_contract, drops_contract.value);
//    auto               epoch_itr = epochs.find(epoch);
//    check(epoch_itr != epochs.end(), "Epoch does not exist");

//    epoch::epoch_table epoch(_self, _self.value);
//    auto               epoch_itr = epoch.find(epoch);
//    check(epoch_itr != epoch.end(), "Oracle Epoch does not exist");

//    vector<name>        has_revealed;
//    epoch::reveal_table reveals(_self, _self.value);
//    auto                reveals_idx = reveals.get_index<"epochoracle"_n>();
//    for (name oracle : epoch_itr->oracles) {
//       auto completed_reveals_itr = reveals_idx.find(((uint128_t)oracle.value << 64) + epoch);
//       if (completed_reveals_itr != reveals_idx.end()) {
//          has_revealed.push_back(oracle);
//       }
//    }

//    if (has_revealed.size() == epoch_itr->oracles.size()) {
//       // Complete the epoch
//       epoch.modify(epoch_itr, _self, [&](auto& row) {
//          row.completed = 1;
//          row.drops     = compute_epoch_value(epoch);
//       });
//    }
// }

[[eosio::action]] void epoch::addoracle(name oracle)
{
   require_auth(_self);
   check(is_account(oracle), "Account does not exist.");
   epoch::oracle_table oracles(_self, _self.value);
   oracles.emplace(_self, [&](auto& row) { row.oracle = oracle; });
}

[[eosio::action]] void epoch::removeoracle(name oracle)
{
   require_auth(_self);

   epoch::oracle_table oracles(_self, _self.value);
   auto                oracle_itr = oracles.find(oracle.value);
   check(oracle_itr != oracles.end(), "Oracle not found");
   oracles.erase(oracle_itr);
}

// [[eosio::action]] void epoch::subscribe(name subscriber)
// {
//    // TODO: Maybe this needs to require the oracles or _self?
//    // The person advancing I think needs to pay for the CPU to notify the other
//    // contracts
//    require_auth(_self);

//    epoch::subscriber_table subscribers(_self, _self.value);
//    auto                    subscriber_itr = subscribers.find(subscriber.value);
//    check(subscriber_itr == subscribers.end(), "Already subscribed to notifictions.");
//    subscribers.emplace(_self, [&](auto& row) { row.subscriber = subscriber; });
// }

// [[eosio::action]] void epoch::unsubscribe(name subscriber)
// {
//    require_auth(subscriber);

//    epoch::subscriber_table subscribers(_self, _self.value);
//    auto                    subscriber_itr = subscribers.find(subscriber.value);
//    check(subscriber_itr != subscribers.end(), "Not currently subscribed.");
//    subscribers.erase(subscriber_itr);
// }

[[eosio::action]] void epoch::init()
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();

   const block_timestamp genesis =
      time_point_sec((current_time_point().sec_since_epoch() / state.epochlength) * state.epochlength);

   state.enabled = true;
   state.genesis = genesis;
   _state.set(state, get_self());

   // Load oracles to initialize the first epoch
   epoch::oracle_table oracle_table(_self, _self.value);
   std::vector<name>   oracles;
   auto                oracle_itr = oracle_table.begin();
   check(oracle_itr != oracle_table.end(), "No oracles registered, cannot init.");
   while (oracle_itr != oracle_table.end()) {
      oracles.push_back(oracle_itr->oracle);
      oracle_itr++;
   }

   // Add the initial epoch row to the oracle contract
   epoch::epoch_table epochs(_self, _self.value);
   epochs.emplace(_self, [&](auto& row) {
      row.epoch     = 1;
      row.oracles   = oracles;
      row.completed = 0;
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
[[eosio::action]] void epoch::epochlength(const uint32_t epochlength)
{
   require_auth(get_self());

   epoch::state_table _state(get_self(), get_self().value);
   auto               state = _state.get_or_default();
   state.epochlength        = epochlength;
   _state.set(state, get_self());
}

[[eosio::action]] void epoch::wipe()
{
   require_auth(_self);

   epoch::state_table _state(get_self(), get_self().value);
   _state.remove();
   auto state = _state.get_or_default();
   _state.set(state, get_self());

   epoch::commit_table commits(_self, _self.value);
   auto                commit_itr = commits.begin();
   while (commit_itr != commits.end()) {
      commit_itr = commits.erase(commit_itr);
   }

   epoch::epoch_table epochs(_self, _self.value);
   auto               epoch_itr = epochs.begin();
   while (epoch_itr != epochs.end()) {
      epoch_itr = epochs.erase(epoch_itr);
   }

   //    epoch::reveal_table reveals(_self, _self.value);
   //    auto                reveal_itr = reveals.begin();
   //    while (reveal_itr != reveals.end()) {
   //       reveal_itr = reveals.erase(reveal_itr);
   //    }

   epoch::oracle_table oracles(_self, _self.value);
   auto                oracle_itr = oracles.begin();
   while (oracle_itr != oracles.end()) {
      oracle_itr = oracles.erase(oracle_itr);
   }

   //    epoch::subscriber_table subscribers(_self, _self.value);
   //    auto                    subscribers_itr = subscribers.begin();
   //    while (subscribers_itr != subscribers.end()) {
   //       subscribers_itr = subscribers.erase(subscribers_itr);
   //    }
}

epoch::epoch_row epoch::advance_epoch()
{
   epoch::state_table _state(get_self(), get_self().value);
   auto               state_itr = _state.get_or_default();

   const uint64_t epoch = derive_epoch(state_itr.genesis, state_itr.epochlength);

   // Retrieve current epoch from oracle contract
   epoch::epoch_table epochs(_self, _self.value);
   auto               epochs_itr = epochs.find(epoch);
   check(epochs_itr == epochs.end(), "Epoch " + std::to_string(epoch) + " from oracle contract state already exists.");

   // Find currently active oracles
   std::vector<name>   oracles;
   epoch::oracle_table oracle_table(_self, _self.value);
   auto                oracle_itr = oracle_table.begin();
   check(oracle_itr != oracle_table.end(), "No active oracles - cannot advance.");
   while (oracle_itr != oracle_table.end()) {
      oracles.push_back(oracle_itr->oracle);
      oracle_itr++;
   }

   // Save the next epoch to the oracle contract
   epochs.emplace(_self, [&](auto& row) {
      row.epoch     = epoch;
      row.oracles   = oracles;
      row.completed = 0;
   });

   // Nofify subscribers
   epoch::subscriber_table subscribers(_self, _self.value);
   auto                    subscriber_itr = subscribers.begin();
   while (subscriber_itr != subscribers.end()) {
      require_recipient(subscriber_itr->subscriber);
      subscriber_itr++;
   }

   // Return the next epoch
   return {
      epoch,   // epoch
      oracles, // oracles
      0        // completed
   };
}

// [[eosio::action]] epoch::epoch_row epoch::advance()
// {
//    // Only the drops contract can advance the oracle contract
//    require_auth(_self);

//    // Advance the epoch
//    auto new_epoch = advance_epoch();

//    // Provide the epoch as a return value
//    return new_epoch;
// }

} // namespace dropssystem
