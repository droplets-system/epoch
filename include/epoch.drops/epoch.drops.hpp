#pragma once

#include <cmath>
#include <drops/drops.hpp>
#include <eosio.system/eosio.system.hpp>

using namespace eosio;
using namespace std;

static constexpr name drops_contract = "seed.gm"_n; // location of drops contract

static const string ERROR_SYSTEM_DISABLED = "Drops system is disabled.";

namespace dropssystem {

class [[eosio::contract("epoch.drops")]] epoch : public contract
{
public:
   using contract::contract;

   struct [[eosio::table("epoch")]] epoch_row
   {
      uint64_t          epoch;
      std::vector<name> oracles;
      uint64_t          completed;
      checksum256       seed;
      uint64_t          primary_key() const { return epoch; }
      uint64_t          by_completed() const { return completed; }
   };

   struct [[eosio::table("commit")]] commit_row
   {
      uint64_t    id;
      uint64_t    epoch;
      name        oracle;
      checksum256 commit;
      uint64_t    primary_key() const { return id; }
      uint64_t    by_epoch() const { return epoch; }
      uint128_t   by_epochoracle() const { return ((uint128_t)oracle.value << 64) | epoch; }
   };

   struct [[eosio::table("oracle")]] oracle_row
   {
      name     oracle;
      uint64_t primary_key() const { return oracle.value; }
   };

   struct [[eosio::table("reveal")]] reveal_row
   {
      uint64_t  id;
      uint64_t  epoch;
      name      oracle;
      string    reveal;
      uint64_t  primary_key() const { return id; }
      uint64_t  by_epoch() const { return epoch; }
      uint128_t by_epochoracle() const { return ((uint128_t)oracle.value << 64) | epoch; }
   };

   struct [[eosio::table("state")]] state_row
   {
      block_timestamp genesis     = current_block_time();
      uint32_t        epochlength = 86400; // Epoch duration, 1-day default
      bool            enabled     = false;
   };

   struct [[eosio::table("subscriber")]] subscriber_row
   {
      name     subscriber;
      uint64_t primary_key() const { return subscriber.value; }
   };

   typedef eosio::multi_index<"subscriber"_n, subscriber_row> subscriber_table;
   typedef eosio::multi_index<"oracle"_n, oracle_row>         oracle_table;
   typedef eosio::multi_index<
      "epoch"_n,
      epoch_row,
      eosio::indexed_by<"completed"_n, eosio::const_mem_fun<epoch_row, uint64_t, &epoch_row::by_completed>>>
      epoch_table;
   typedef eosio::multi_index<
      "commit"_n,
      commit_row,
      eosio::indexed_by<"epoch"_n, eosio::const_mem_fun<commit_row, uint64_t, &commit_row::by_epoch>>,
      eosio::indexed_by<"epochoracle"_n, eosio::const_mem_fun<commit_row, uint128_t, &commit_row::by_epochoracle>>>
                                                  commit_table;
   typedef eosio::singleton<"state"_n, state_row> state_table;
   typedef eosio::multi_index<
      "reveal"_n,
      reveal_row,
      eosio::indexed_by<"epoch"_n, eosio::const_mem_fun<reveal_row, uint64_t, &reveal_row::by_epoch>>,
      eosio::indexed_by<"epochoracle"_n, eosio::const_mem_fun<reveal_row, uint128_t, &reveal_row::by_epochoracle>>>
      reveal_table;

   /*
    Oracle actions
   */
   [[eosio::action]] void commit(name oracle, uint64_t epoch, checksum256 commit);
   using commit_action = eosio::action_wrapper<"commit"_n, &epoch::commit>;

   //    [[eosio::action]] void reveal(name oracle, uint64_t epoch, string reveal);
   //    using reveal_action = eosio::action_wrapper<"reveal"_n, &epoch::reveal>;

   //    [[eosio::action]] void finishreveal(uint64_t epoch);
   //    using finishreveal_action = eosio::action_wrapper<"finishreveal"_n, &epoch::finishreveal>;

   //    [[eosio::action]] void subscribe(name subscriber);
   //    using subscribe_action = eosio::action_wrapper<"subscribe"_n, &epoch::subscribe>;

   //    [[eosio::action]] void unsubscribe(name subscriber);
   //    using unsubscribe_action = eosio::action_wrapper<"unsubscribe"_n, &epoch::unsubscribe>;

   /*

    Admin actions

   */
   [[eosio::action]] void addoracle(name oracle);
   using addoracle_action = eosio::action_wrapper<"addoracle"_n, &epoch::addoracle>;

   [[eosio::action]] void removeoracle(name oracle);
   using removeoracle_action = eosio::action_wrapper<"removeoracle"_n, &epoch::removeoracle>;

   [[eosio::action]] void init();
   using init_action = eosio::action_wrapper<"init"_n, &epoch::init>;

   [[eosio::action]] void enable(const bool enabled);
   using enable_action = eosio::action_wrapper<"enable"_n, &epoch::enable>;

   [[eosio::action]] void epochlength(const uint32_t epochlength);
   using epochlength_action = eosio::action_wrapper<"epochlength"_n, &epoch::epochlength>;

   //    [[eosio::action]] epoch_row advance();
   //    using advance_action = eosio::action_wrapper<"advance"_n, &epoch::advance>;

   /*

   Testnet actions

   */
   [[eosio::action]] void wipe();
   using wipe_action = eosio::action_wrapper<"wipe"_n, &epoch::wipe>;

   // 1706486401 -
   // 1706486400
   // / 86400
   // (1706486401 - 1706486400) / 86400

   /*

    Computation helpers

   */
   //    [[eosio::action]] checksum256 computeepoch(uint64_t epoch);
   //    using computeepoch_action = eosio::action_wrapper<"computeepoch"_n, &epoch::computeepoch>;

   //    [[eosio::action]] checksum256 computedrops(uint64_t epoch, uint64_t drops);
   //    using computedrops_action = eosio::action_wrapper<"computedrops"_n, &epoch::computedrops>;

   //    [[eosio::action]] checksum256 cmplastepoch(uint64_t drops, name contract);
   //    using cmplastepoch_action = eosio::action_wrapper<"cmplastepoch"_n, &epoch::cmplastepoch>;

   //    checksum256 compute_epoch_value(uint64_t epoch);
   //    checksum256 compute_epoch_drops_value(uint64_t epoch, uint64_t drops);
   //    checksum256 compute_last_epoch_drops_value(uint64_t drops);

   static constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

   static uint64_t derive_epoch(const block_timestamp genesis, const uint32_t epochlength)
   {
      return std::floor((current_time_point().sec_since_epoch() - genesis.to_time_point().sec_since_epoch()) /
                        epochlength) +
             1;
   }

   static std::string hexStr(unsigned char* data, int len)
   {
      std::string s(len * 2, ' ');
      for (int i = 0; i < len; ++i) {
         s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
         s[2 * i + 1] = hexmap[data[i] & 0x0F];
      }
      return s;
   }

   static uint16_t clz(checksum256 checksum)
   {
      auto                 byte_array    = checksum.extract_as_byte_array();
      const uint8_t*       my_bytes      = (uint8_t*)byte_array.data();
      size_t               size          = byte_array.size();
      size_t               lzbits        = 0;
      static const uint8_t char2lzbits[] = {
         // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22
         // 23 24 25 26 27 28 29 30 31
         8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

      size_t i = 0;

      while (true) {
         uint8_t c = my_bytes[i];
         lzbits += char2lzbits[c];
         if (c != 0)
            break;
         ++i;
         if (i >= size)
            return 0x100;
      }

      return lzbits;
   }

   static checksum256 hash(checksum256 epochdrops, uint64_t drops)
   {
      // Combine the epoch drops and drops into a single string
      auto   epoch_arr = epochdrops.extract_as_byte_array();
      string result    = hexStr(epoch_arr.data(), epoch_arr.size()) + std::to_string(drops);

      // Generate the sha256 value of the combined string
      return sha256(result.c_str(), result.length());
   }

// DEBUG (used to help testing)
#ifdef DEBUG
   [[eosio::action]] void test(const string data);

   // @debug
   [[eosio::action]] void
   cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);
#endif

private:
   void check_is_enabled();

   epoch::epoch_row advance_epoch();
   void             ensure_epoch_advance(epoch::state_row state);

// DEBUG (used to help testing)
#ifdef DEBUG
   template <typename T>
   void clear_table(T& table, uint64_t rows_to_clear);
#endif
};

} // namespace dropssystem
