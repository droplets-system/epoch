namespace dropssystem {

// @debug
[[eosio::action]] void epoch::test(const string data) { print(data); }

// @debug
template <typename T>
void epoch::clear_table(T& table, uint64_t rows_to_clear)
{
   auto itr = table.begin();
   while (itr != table.end() && rows_to_clear--) {
      itr = table.erase(itr);
   }
}

// @debug
[[eosio::action]] void
epoch::cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows)
{
   require_auth(get_self());
   const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
   const uint64_t value         = scope ? scope->value : get_self().value;

   // tables
   epoch::state_table  _state(get_self(), value);
   epoch::oracle_table _oracle(get_self(), value);

   if (table_name == "oracle"_n)
      clear_table(_oracle, rows_to_clear);
   else if (table_name == "state"_n)
      _state.remove();
   else
      check(false, "cleartable: [table_name] unknown table to clear");
}

} // namespace dropssystem