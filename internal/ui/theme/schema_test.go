package theme

import (
	"bytes"
	"encoding/json"
	"os"
	"reflect"
	"sort"
	"strings"
	"testing"
)

// The JSON Schema is published for theme authors (FR-UI-5). It must accept
// what Load accepts and reject what Load rejects where JSON Schema can
// express it, otherwise editors flag valid themes or pass broken ones. These
// tests pin the schema to the loader.

type schemaNode struct {
	Required             []string               `json:"required"`
	Properties           map[string]*schemaNode `json:"properties"`
	AdditionalProperties json.RawMessage        `json:"additionalProperties"`
	PropertyNames        *schemaNode            `json:"propertyNames"`
	Pattern              string                 `json:"pattern"`
	Enum                 []string               `json:"enum"`
	MinLength            *int                   `json:"minLength"`
	Minimum              *float64               `json:"minimum"`
	Maximum              *float64               `json:"maximum"`
	Defs                 map[string]*schemaNode `json:"$defs"`
}

func loadSchema(t *testing.T) *schemaNode {
	t.Helper()
	b, err := os.ReadFile("themes/sdr-next-theme.schema.json")
	if err != nil {
		t.Fatal(err)
	}
	var s schemaNode
	if err := json.Unmarshal(b, &s); err != nil {
		t.Fatal(err)
	}
	return &s
}

func sorted(s []string) []string {
	out := append([]string(nil), s...)
	sort.Strings(out)
	return out
}

func colourKeys() []string {
	var c Colors
	var keys []string
	for k := range c.fields() {
		keys = append(keys, k)
	}
	return sorted(keys)
}

func TestSchemaRequiresOnlyWhatLoadRequires(t *testing.T) {
	s := loadSchema(t)
	want := []string{"base", "format", "id", "name"}
	if got := sorted(s.Required); !reflect.DeepEqual(got, want) {
		t.Errorf("schema required = %v, loader requires %v (the rest falls back to base)", got, want)
	}
	// A user theme without "vfo" or "waterfall" is valid for Load.
	src := `{"format":1,"id":"night-red","name":"Ночная","base":"dark","colors":{"dial":"#ff5a3c"}}`
	if _, err := Load(strings.NewReader(src)); err != nil {
		t.Fatalf("partial theme rejected by Load: %v", err)
	}
}

func TestSchemaColourKeysMatchLoader(t *testing.T) {
	var got []string
	for k := range loadSchema(t).Properties["colors"].Properties {
		got = append(got, k)
	}
	if want := colourKeys(); !reflect.DeepEqual(sorted(got), want) {
		t.Errorf("schema colours %v, loader colours %v", sorted(got), want)
	}
}

func TestSchemaFontsMatchLoader(t *testing.T) {
	fonts := loadSchema(t).Properties["fonts"]
	if fonts.PropertyNames == nil || !reflect.DeepEqual(sorted(fonts.PropertyNames.Enum), []string{"mono", "sans"}) {
		t.Error(`fonts keys must be limited to "sans" and "mono"`)
	}
	var font schemaNode
	if err := json.Unmarshal(fonts.AdditionalProperties, &font); err != nil {
		t.Fatal(err)
	}
	if f := font.Properties["family"]; f == nil || f.MinLength == nil || *f.MinLength != 1 {
		t.Error("font family must be non-empty (minLength 1)")
	}
	if !reflect.DeepEqual(sorted(font.Required), []string{"family"}) {
		t.Errorf("font required = %v, want [family]", font.Required)
	}
	for name, src := range map[string]string{
		"unknown family key": `{"format":1,"id":"x","name":"x","base":"dark","fonts":{"serif":{"family":"X"}}}`,
		"empty family":       `{"format":1,"id":"x","name":"x","base":"dark","fonts":{"sans":{"family":""}}}`,
	} {
		if _, err := Load(strings.NewReader(src)); err == nil {
			t.Errorf("%s: expected Load to reject it", name)
		}
	}
}

func TestSchemaTokenNamesMatchLoader(t *testing.T) {
	s := loadSchema(t)
	for name, n := range map[string]*schemaNode{
		"text":    s.Properties["text"],
		"opacity": s.Properties["opacity"],
		"numMap":  s.Defs["numMap"],
	} {
		if n.PropertyNames == nil || n.PropertyNames.Pattern != idRe.String() {
			t.Errorf("%s: propertyNames.pattern must be %s", name, idRe.String())
		}
	}
}

func TestSchemaFocusRingAndShadowMatchLoader(t *testing.T) {
	s := loadSchema(t)
	fr := s.Properties["focusRing"]
	if !reflect.DeepEqual(sorted(fr.Required), []string{"color", "gap", "width"}) {
		t.Errorf("focusRing required = %v: Load rejects a partial focusRing", fr.Required)
	}
	if !reflect.DeepEqual(sorted(fr.Properties["color"].Enum), colourKeys()) {
		t.Error("focusRing.color must enumerate the colour keys")
	}
	sh := s.Properties["floatShadow"]
	if !reflect.DeepEqual(sorted(sh.Required), []string{"color"}) {
		t.Errorf("floatShadow required = %v, want [color]", sh.Required)
	}
	if y := sh.Properties["offsetY"]; y.Minimum == nil || *y.Minimum != -64 || y.Maximum == nil || *y.Maximum != 64 {
		t.Error("floatShadow.offsetY must be limited to [-64, 64]")
	}
	for name, src := range map[string]string{
		"partial focusRing":    `{"format":1,"id":"x","name":"x","base":"dark","focusRing":{"gap":2}}`,
		"shadow without color": `{"format":1,"id":"x","name":"x","base":"dark","floatShadow":{"blur":4}}`,
		"shadow offset":        `{"format":1,"id":"x","name":"x","base":"dark","floatShadow":{"color":"#000000","offsetY":65}}`,
	} {
		if _, err := Load(strings.NewReader(src)); err == nil {
			t.Errorf("%s: expected Load to reject it", name)
		}
	}
}

// Built-in themes are also valid user themes, so they can serve as
// templates for theme authors.
func TestBuiltinThemesAreValidUserThemes(t *testing.T) {
	for _, base := range []string{"dark", "light"} {
		b, err := builtinFS.ReadFile("themes/sdr-next-" + base + ".json")
		if err != nil {
			t.Fatal(err)
		}
		if _, err := Load(bytes.NewReader(b)); err != nil {
			t.Errorf("%s: %v", base, err)
		}
	}
}
