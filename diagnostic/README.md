# Diagnostic build (SANAE) - why the save is not kept

Same port, one difference: the runner's three silent "the account save could
not be mounted" paths now execute a breakpoint instead of writing to a log
nobody can read.

```
brk #0xA1   no user selected
brk #0xA2   cannot open the last user
brk #0xA3   not enough space to create the save
```

Install it over the normal build (same title id), start the game, play until it
saves (start a new game), then:

* **it crashes** -> the crash report names the reason. The `Break Address` /
  `PC` will point at `VCTemplate.nss + 0x5a230`, `+ 0x5a24c` or `+ 0x5a284`
  and the "Break Reason" field carries the number above. Send the report.
* **it does not crash** -> the save data *is* mounted, so the problem is the
  commit, not the mount, and the next fix goes there.

Afterwards reinstall the normal NSP.
