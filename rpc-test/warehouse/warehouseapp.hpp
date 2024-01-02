#pragma once
#include <map>
#include <string>
#include <vector>
#include "types.h"

//#include "nlohmann/json.hpp"

class WarehouseServer {
public:
  WarehouseServer() :
    products() {}

  bool AddProduct(const Product &p);
  bool TestProduct(const nlohmann::json p);
  const Product& GetProduct(const std::string& id);
  std::vector<Product> AllProducts();

private:
  std::map<std::string, Product> products;
};

