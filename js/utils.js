import { setTimeout, clearTimeout } from 'os';

const intervals = {}

export function clearInterval(interval) {
  const i = intervals[interval];
  i && clearTimeout(i);
  delete intervals[interval];
}

export function setInterval(fn, ms) {
  const interval = Symbol();
  const newFn = () => {
    fn();
    intervals[interval] = setTimeout(newFn, ms);
  }
  intervals[interval] = setTimeout(newFn, ms);
  return interval;
}