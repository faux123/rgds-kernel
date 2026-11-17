# RG DS device tree reconstruction tools

These scripts rebuilt `arch/arm64/boot/dts/rockchip/rk3566-anbernic-rg-ds.dts`
from the DTB in the stock Android 14 V1.21 boot image (RSCE resource,
sha256 `3960849133cca459ccae70c5d96649c7de8c52d1df42fb7288231fb072d0d709`),
and they check that the source still compiles to the same tree.

## Check that the board dts matches stock (the 0 gate)

1. Build the DTB:

   ```sh
   make ARCH=arm64 LLVM=1 rockchip/rk3566-anbernic-rg-ds.dtb
   ```

2. Compare it with the stock DTB:

   ```sh
   scripts/rgds/dts/gate.sh STOCK.dtb arch/arm64/boot/dts/rockchip/rk3566-anbernic-rg-ds.dtb
   ```

   The script prints the number of differing lines. `0` means the two trees
   are equivalent. The full diff is in `$TMPDIR/rgds-gate.diff`.

Raw bytes differ because dtc numbers phandles differently when it compiles
from source. `norm.py` therefore replaces every phandle reference with the
path of the node it points to, then sorts nodes and properties.

## Scripts

`norm.py DTB`
: Dumps a DTB as sorted text with phandle references resolved to node paths.

`normlib.py`
: The DTB parser and the reference resolver shared by the other scripts. It
  knows the standard `#*-cells` properties, `pinctrl-N`, `*-gpio(s)`,
  `*-supply`, `interrupt-map`, `msi-map`, `rockchip,pins`, and the Rockchip
  vendor properties that hold phandles.

`gen.py BASE.dtb STOCK.dtb HEADER`
: Writes a board dts: `HEADER` (the `#include` lines) followed by the
  overrides, new nodes, and deletions that turn `BASE.dtb` into `STOCK.dtb`.
  `BASE.dtb` is the SoC dtsi compiled alone with `dtc -f -@`.

`audit.py STOCK.dtb BUILT.dtb`
: Lists properties that the resolver leaves numeric but that contain a value
  equal to a stock phandle. A property in this list that also appears with
  raw numbers in the board dts may be an unresolved reference, which the gate
  cannot see. Add such properties to `normlib.py`.
