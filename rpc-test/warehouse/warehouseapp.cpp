#include "warehouseapp.hpp"
#include <jsonrpccxx/common.hpp>
#include <iostream>
using namespace jsonrpccxx;

bool WarehouseServer::AddProduct(const Product &p) {
  if (products.find(p.id) != products.end())
    return false;
  products[p.id] = p;
  return true;
}
/*
struct Product {
public:
  Product() : id(), price(), name(), cat() {}
  std::string id;
  double price;
  std::string name;
  category cat;
};

*/
bool WarehouseServer::TestProduct(const nlohmann::json p) {
  Product ret;
  ret.id = p["id"];
  ret.price = p["price"];
  ret.name = p["name"];
  std::cout << "Got product" << p.dump(4) << std::endl;
  products[ret.id] = ret;
  return true;
}

const Product& WarehouseServer::GetProduct(const std::string &id) {
  if (products.find(id) == products.end())
    throw JsonRpcException(-33000, "No product listed for id: " + id);
  return products[id];
}
std::vector<Product> WarehouseServer::AllProducts() {
  std::vector<Product> result;
  for (const auto &p : products)
    result.push_back(p.second);
  return result;
}
