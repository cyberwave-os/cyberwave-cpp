/**
 * @brief Edge views and edge management helpers for the Cyberwave SDK.
 */

#ifndef CYBERWAVE_EDGES_H
#define CYBERWAVE_EDGES_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cyberwave
{

class Client;

/**
 * @brief Lightweight view over an edge returned by the backend.
 */
class Edge
{
public:
    /** @brief Construct an edge view from a backend payload. */
    explicit Edge(std::shared_ptr<void> schema_ptr);

    /** @brief Return the edge UUID. */
    std::string uuid() const;

    /** @brief Return the edge name. */
    std::string name() const;

    /** @brief Return the edge fingerprint. */
    std::string fingerprint() const;

private:
    std::shared_ptr<void> schema_;
};

/**
 * @brief Manager for edge CRUD operations.
 */
class EdgeManager
{
public:
    /** @brief Construct an edge manager bound to a client. */
    explicit EdgeManager(const Client& client);

    /** @brief List all edges visible to the client. */
    std::vector<Edge> list() const;

    /** @brief Fetch an edge by UUID. */
    Edge get(const std::string& edge_id) const;

    /** @brief Create a new edge record. */
    Edge create(const std::string& fingerprint, const std::string& name = "", const std::string& workspace_id = "",
                const std::map<std::string, std::string>& metadata = {}) const;

    /** @brief Update mutable edge fields. */
    Edge update(const std::string& edge_id, const std::string& name = "",
                const std::map<std::string, std::string>& metadata = {}) const;

    /** @brief Delete an edge by UUID. */
    void delete_edge(const std::string& edge_id) const;

private:
    std::reference_wrapper<const Client> client_;
};

} // namespace cyberwave

#endif // CYBERWAVE_EDGES_H
