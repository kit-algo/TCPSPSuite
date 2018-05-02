#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <set>                               // for set
#include "instance.hpp"  // for Instance
#include "traits.hpp"                        // for TraitsRouter, TraitsRout...

class Transformer {
public:
  explicit Transformer();
  // go away from pointers here.
  virtual void run(const Instance & input) = 0;
  virtual Instance get_transformed() = 0;

  virtual TraitsRouter::transform_profile get_transformation() const = 0;
  virtual TraitsRouter::transform_profile get_requirements() const = 0;

protected:
};

class SameStartEndTransformer : public Transformer {
public:
  explicit SameStartEndTransformer();
  virtual void run(const Instance & input);
  virtual Instance get_transformed();

  virtual TraitsRouter::transform_profile get_transformation() const;
  virtual TraitsRouter::transform_profile get_requirements() const;

private:
  Instance transformed;
};

class DummyStartEndTransformer : public Transformer {
public:
  explicit DummyStartEndTransformer();
  virtual void run(const Instance & input);
  virtual Instance get_transformed();

  virtual TraitsRouter::transform_profile get_transformation() const;
  virtual TraitsRouter::transform_profile get_requirements() const;
private:
  Instance transformed;
};

class ConsistentWindowTransformer : public Transformer {
public:
  explicit ConsistentWindowTransformer();
  virtual void run(const Instance & input);
  virtual Instance get_transformed();

  virtual TraitsRouter::transform_profile get_transformation() const;
  virtual TraitsRouter::transform_profile get_requirements() const;
private:
  Instance transformed;
};

class TransformerManager {
public:
  static const TransformerManager& get();

  const std::set<Transformer *> get_all() const;

  ~TransformerManager();

private:
  TransformerManager();

  static std::set<Transformer *> all_transformers;
};

#endif
