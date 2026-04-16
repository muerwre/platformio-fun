#pragma once

class Reporter
{
public:
  virtual ~Reporter() = default;
  virtual int report() = 0;
};
