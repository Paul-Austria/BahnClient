using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace ModellClientLib.Migrations
{
    public partial class uptdatedSegment3 : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.AddColumn<string>(
                name: "SegmentGroup",
                table: "segments",
                type: "TEXT",
                nullable: false,
                defaultValue: "");
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropColumn(
                name: "SegmentGroup",
                table: "segments");
        }
    }
}
