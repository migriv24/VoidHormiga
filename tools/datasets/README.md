# tools/datasets — large databases for stress-testing

Not part of Hormiga. These tools build `.miga` files big enough to find where
Hormiga stops being usable, out of public data plus synthetic people. Nothing
real about a person goes in, and nothing they build is committed: the
downloads and the output live outside the repository, wherever you point them.

## stressgen

```
cmake --build build --target hormiga_stressgen
hormiga_stressgen --raw <downloads> --out <folder> [--total 500000] [--contacts 150000]
                  [--seed 7] [--cluster-size 2000] [--name stress-500k] [--validate]
```

It makes one `.miga` whose data mantle holds:

| what | from | kept | left out |
|---|---|---|---|
| organizations | IRS Exempt Organizations Business Master File (public domain) | name, city, state, ZIP, NTEE field, revenue, assets | the "in care of" name and the street: either can name a person or a home |
| where they are | Census 2020 ZCTA gazetteer (public domain) | ZIP centroid, jittered by about a kilometre | |
| contacts | synthetic | names from fixed lists, `example.org` emails, `555-01xx` phones | |
| events | synthetic | twenty templates over 2026–2027 | |

The links have the structure of a real outreach database. A contact is a
`member` of one organization (weighted toward larger ones) and `knows` colleagues
there. Some contacts also volunteer at a second organization in the same city.
An event is `hosted_by` an organization, and its members attend. Organizations
`partner` within a city and a field. Revenue and assets are values on edges to
two measure runes (okf/concepts/foundation/measures.md).

**Clusters.** Void Maiz's Leiden hierarchy (`voidmaiz/graph.hpp`) runs over the
structural links. Value edges are left out, because a value and a strength are
not the same kind of number. Every rune is tagged with its cluster at each
level: `c1:12`, `c2:12.4`, and so on, down to `--cluster-size`. The run prints
modularity, and the NMI of level 1 against city and against field. That shows
how much real structure the clustering recovered.

**Size.** `--total` is the number of runes. When the real organizations would
not fit, a random sample of them is kept, so the clusters span every state
downloaded, not just the first.

**`--validate`.** Loads the file into a real Void Core, projects it back, and
checks that every rune and link survived. It also prints what the load, one
export and one projection cost, and how big the export becomes after a `save`.

### The downloads

Put these in one folder and pass it as `--raw`:

- `2020_Gaz_zcta_national.txt`, from the Census gazetteer files (unzip it);
- one or more `eo_<state>.csv`, from the IRS SOI exempt organization extracts.

The output is deterministic: the same downloads and `--seed` give the same state
document. Only the envelope's `saved` time differs.

### What it found

The 5×10^5 measurements, and what changed because of them, are in
okf/concepts/platform/scale.md.
