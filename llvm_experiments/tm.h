#pragma once

extern "C"
void __start_transaction();
extern "C"
void __end_transaction();
extern "C"
int __get_hash();